#include "veyra/runtime/route_service.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace veyra {
namespace {

std::string SanitizeProtocolField(const std::string& value) {
  std::string sanitized = value;
  for (char& ch : sanitized) {
    if (ch == '\t' || ch == '\n' || ch == '\r') {
      ch = ' ';
    }
  }
  return sanitized;
}

std::vector<std::string> Split(const std::string& input, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream stream(input);
  while (std::getline(stream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string Join(const std::vector<std::string>& values, const std::string& delimiter) {
  std::ostringstream output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    output << values[index];
    if (index + 1 < values.size()) {
      output << delimiter;
    }
  }
  return output.str();
}

std::string JsonEscape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

bool ParseRouteFields(const std::vector<std::string>& fields,
                      RouteRuntimeState* route,
                      std::string* error) {
  if (fields.size() < 16) {
    if (error != nullptr) {
      *error = "Route service returned an incomplete ROUTE record.";
    }
    return false;
  }

  if (route == nullptr) {
    if (error != nullptr) {
      *error = "Route service route output target was null.";
    }
    return false;
  }

  *route = RouteRuntimeState{
      fields[1],
      fields[2],
      fields[3],
      fields[4],
      fields[5],
      fields[6],
      fields[7],
      fields[8],
      fields[9],
      fields[10],
      fields[11],
      fields[12],
      fields[13],
      fields.size() > 14 ? fields[14] : std::string(),
      fields.size() > 15 ? Split(fields[15], ',') : std::vector<std::string>(),
  };
  return true;
}

void CloseFd(int* fd) {
  if (fd != nullptr && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

}  // namespace

RouteServiceClient::RouteServiceClient(std::string binary_path)
    : binary_path_(std::move(binary_path)) {}

RouteServiceClient::~RouteServiceClient() {
  Shutdown();
}

bool RouteServiceClient::Bootstrap(const std::vector<RuntimeProfile>& profiles,
                                   const std::string& runtime_root,
                                   std::string* error) {
  routes_.clear();

  if (!Start(error)) {
    return false;
  }

  if (!SendLine("BOOTSTRAP\t" + SanitizeProtocolField(runtime_root), error)) {
    return false;
  }

  for (const RuntimeProfile& profile : profiles) {
    if (!SendProfileLine("PROFILE", profile, error)) {
      return false;
    }
  }

  if (!SendLine("END", error)) {
    return false;
  }

  while (true) {
    std::string line;
    if (!ReadLine(&line, error)) {
      return false;
    }

    const std::vector<std::string> fields = Split(line, '\t');
    if (fields.empty()) {
      continue;
    }

    if (fields[0] == "ROUTE") {
      RouteRuntimeState route;
      if (!ParseRouteFields(fields, &route, error)) {
        return false;
      }
      routes_.push_back(std::move(route));
      continue;
    }

    if (fields[0] == "OK") {
      return true;
    }

    if (fields[0] == "ERROR") {
      if (error != nullptr) {
        *error = fields.size() > 1 ? fields[1] : "Route service returned an unspecified error.";
      }
      return false;
    }

    if (error != nullptr) {
      *error = "Unexpected route service response: " + line;
    }
    return false;
  }
}

bool RouteServiceClient::SwitchRoute(const RuntimeProfile& profile,
                                     const std::string& runtime_root,
                                     RouteRuntimeState* updated_route,
                                     std::string* error) {
  if (!Start(error)) {
    return false;
  }

  if (!SendLine("BOOTSTRAP\t" + SanitizeProtocolField(runtime_root), error)) {
    return false;
  }

  if (!SendProfileLine("SWITCH", profile, error)) {
    return false;
  }

  RouteRuntimeState route;
  if (!ReadRouteResponse(&route, true, error)) {
    return false;
  }

  ReplaceOrAppendRoute(route);
  if (updated_route != nullptr) {
    *updated_route = route;
  }
  return true;
}

bool RouteServiceClient::RequestStatus(const std::string& persona_id,
                                       RouteRuntimeState* route,
                                       std::string* error) {
  if (!Start(error)) {
    return false;
  }

  if (!SendLine("STATUS\t" + SanitizeProtocolField(persona_id), error)) {
    return false;
  }

  RouteRuntimeState latest_route;
  if (!ReadRouteResponse(&latest_route, true, error)) {
    return false;
  }

  ReplaceOrAppendRoute(latest_route);
  if (route != nullptr) {
    *route = latest_route;
  }
  return true;
}

const RouteRuntimeState* RouteServiceClient::FindByPersonaId(const std::string& persona_id) const {
  for (const RouteRuntimeState& route : routes_) {
    if (route.persona_id == persona_id) {
      return &route;
    }
  }
  return nullptr;
}

const std::vector<RouteRuntimeState>& RouteServiceClient::routes() const {
  return routes_;
}

std::vector<ValidationIssue> RouteServiceClient::WriteRouteStateReport(
    const std::string& report_path) const {
  std::vector<ValidationIssue> issues;

  std::ofstream output(report_path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    issues.push_back({"route_service",
                      "Failed to open route state report for writing: " + report_path});
    return issues;
  }

  output << "{\n"
         << "  \"routes\": [\n";

  for (std::size_t index = 0; index < routes_.size(); ++index) {
    const RouteRuntimeState& route = routes_[index];
    output << "    {\n"
           << "      \"persona_id\": \"" << JsonEscape(route.persona_id) << "\",\n"
           << "      \"persona_name\": \"" << JsonEscape(route.persona_name) << "\",\n"
           << "      \"route_profile_id\": \"" << JsonEscape(route.route_profile_id) << "\",\n"
           << "      \"route_profile_name\": \"" << JsonEscape(route.route_profile_name) << "\",\n"
           << "      \"route_type\": \"" << JsonEscape(route.route_type) << "\",\n"
           << "      \"dns_policy\": \"" << JsonEscape(route.dns_policy) << "\",\n"
           << "      \"webrtc_policy\": \"" << JsonEscape(route.webrtc_policy) << "\",\n"
           << "      \"leak_prevention_level\": \"" << JsonEscape(route.leak_prevention_level)
           << "\",\n"
           << "      \"routing_requirement\": \"" << JsonEscape(route.routing_requirement)
           << "\",\n"
           << "      \"health_status\": \"" << JsonEscape(route.health_status) << "\",\n"
           << "      \"proxy_uri\": \"" << JsonEscape(route.proxy_uri) << "\",\n"
           << "      \"dns_resolver\": \"" << JsonEscape(route.dns_resolver) << "\",\n"
           << "      \"leak_status\": \"" << JsonEscape(route.leak_status) << "\",\n"
           << "      \"diagnostic_summary\": \""
           << JsonEscape(route.diagnostic_summary) << "\",\n"
           << "      \"hops\": [";

    for (std::size_t hop_index = 0; hop_index < route.hops.size(); ++hop_index) {
      output << "\"" << JsonEscape(route.hops[hop_index]) << "\"";
      if (hop_index + 1 < route.hops.size()) {
        output << ", ";
      }
    }

    output << "]\n"
           << "    }";
    if (index + 1 < routes_.size()) {
      output << ",";
    }
    output << "\n";
  }

  output << "  ]\n"
         << "}\n";

  if (!output.good()) {
    issues.push_back({"route_service",
                      "Failed to flush route state report: " + report_path});
  }

  return issues;
}

bool RouteServiceClient::SendProfileLine(const std::string& command,
                                         const RuntimeProfile& profile,
                                         std::string* error) {
  const std::string line =
      command + "\t" + SanitizeProtocolField(profile.persona.id) +
      "\t" + SanitizeProtocolField(profile.persona.display_name) +
      "\t" + SanitizeProtocolField(profile.route_profile.id) +
      "\t" + SanitizeProtocolField(profile.route_profile.display_name) +
      "\t" + SanitizeProtocolField(profile.route_profile.route_type) +
      "\t" + SanitizeProtocolField(profile.route_profile.dns_policy) +
      "\t" + SanitizeProtocolField(profile.route_profile.webrtc_policy) +
      "\t" + SanitizeProtocolField(profile.route_profile.leak_prevention_level) +
      "\t" + SanitizeProtocolField(profile.runtime_policy.routing_requirement) +
      "\t" + SanitizeProtocolField(Join(profile.route_profile.hops, ",")) +
      "\t" + SanitizeProtocolField(profile.route_profile.notes);
  return SendLine(line, error);
}

bool RouteServiceClient::Start(std::string* error) {
  if (child_pid_ > 0) {
    return true;
  }

  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    if (error != nullptr) {
      *error = "Failed to create route service pipes: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]);
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    CloseFd(&stdout_pipe[1]);
    return false;
  }

  child_pid_ = fork();
  if (child_pid_ < 0) {
    if (error != nullptr) {
      *error = "Failed to fork route service: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]);
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    CloseFd(&stdout_pipe[1]);
    child_pid_ = -1;
    return false;
  }

  if (child_pid_ == 0) {
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stdout_pipe[1], STDERR_FILENO);

    CloseFd(&stdin_pipe[0]);
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    CloseFd(&stdout_pipe[1]);

    execl(binary_path_.c_str(), binary_path_.c_str(), "--stdio", nullptr);
    _exit(127);
  }

  CloseFd(&stdin_pipe[0]);
  CloseFd(&stdout_pipe[1]);
  write_fd_ = stdin_pipe[1];
  read_fd_ = stdout_pipe[0];
  return true;
}

void RouteServiceClient::Shutdown() {
  if (child_pid_ > 0) {
    std::string ignored_error;
    SendLine("SHUTDOWN", &ignored_error);
  }

  CloseFd(&write_fd_);
  CloseFd(&read_fd_);

  if (child_pid_ > 0) {
    int status = 0;
    waitpid(child_pid_, &status, 0);
    child_pid_ = -1;
  }
}

bool RouteServiceClient::SendLine(const std::string& line, std::string* error) {
  if (write_fd_ < 0) {
    if (error != nullptr) {
      *error = "Route service write pipe is not available.";
    }
    return false;
  }

  const std::string payload = line + "\n";
  const char* cursor = payload.c_str();
  std::size_t remaining = payload.size();
  while (remaining > 0) {
    const ssize_t written = write(write_fd_, cursor, remaining);
    if (written < 0) {
      if (error != nullptr) {
        *error = "Failed to write to route service: " + std::string(std::strerror(errno));
      }
      return false;
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }

  return true;
}

bool RouteServiceClient::ReadLine(std::string* line, std::string* error) {
  if (read_fd_ < 0) {
    if (error != nullptr) {
      *error = "Route service read pipe is not available.";
    }
    return false;
  }

  line->clear();
  char character = '\0';
  while (true) {
    const ssize_t bytes_read = read(read_fd_, &character, 1);
    if (bytes_read < 0) {
      if (error != nullptr) {
        *error = "Failed to read from route service: " + std::string(std::strerror(errno));
      }
      return false;
    }
    if (bytes_read == 0) {
      if (error != nullptr) {
        *error = "Route service closed unexpectedly.";
      }
      return false;
    }
    if (character == '\n') {
      return true;
    }
    line->push_back(character);
  }
}

bool RouteServiceClient::ReadRouteResponse(RouteRuntimeState* route,
                                           bool expect_ok,
                                           std::string* error) {
  bool saw_route = false;
  while (true) {
    std::string line;
    if (!ReadLine(&line, error)) {
      return false;
    }

    const std::vector<std::string> fields = Split(line, '\t');
    if (fields.empty()) {
      continue;
    }

    if (fields[0] == "ROUTE") {
      RouteRuntimeState parsed_route;
      if (!ParseRouteFields(fields, &parsed_route, error)) {
        return false;
      }
      if (route != nullptr) {
        *route = std::move(parsed_route);
      }
      saw_route = true;
      if (!expect_ok) {
        return true;
      }
      continue;
    }

    if (fields[0] == "OK") {
      if (!saw_route && error != nullptr) {
        *error = "Route service acknowledged without returning route state.";
      }
      return saw_route;
    }

    if (fields[0] == "ERROR") {
      if (error != nullptr) {
        *error = fields.size() > 1 ? fields[1] : "Route service returned an unspecified error.";
      }
      return false;
    }

    if (error != nullptr) {
      *error = "Unexpected route service response: " + line;
    }
    return false;
  }
}

void RouteServiceClient::ReplaceOrAppendRoute(RouteRuntimeState route) {
  for (RouteRuntimeState& existing_route : routes_) {
    if (existing_route.persona_id == route.persona_id) {
      existing_route = std::move(route);
      return;
    }
  }
  routes_.push_back(std::move(route));
}

}  // namespace veyra
