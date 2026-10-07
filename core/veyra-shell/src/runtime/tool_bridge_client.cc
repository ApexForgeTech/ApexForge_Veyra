#include "veyra/runtime/tool_bridge_client.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

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

std::string JsonEscape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\': escaped += "\\\\"; break;
      case '"':  escaped += "\\\""; break;
      case '\n': escaped += "\\n";  break;
      case '\r': escaped += "\\r";  break;
      case '\t': escaped += "\\t";  break;
      default:   escaped.push_back(ch); break;
    }
  }
  return escaped;
}

std::string NowMillisString() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void CloseFd(int* fd) {
  if (fd != nullptr && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

bool VectorContains(const std::vector<std::string>& vec, const std::string& value) {
  for (const std::string& item : vec) {
    if (item == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

ToolBridgeClient::ToolBridgeClient(std::string bridge_binary_path,
                                   std::vector<ToolDefinition> tools)
    : bridge_binary_path_(std::move(bridge_binary_path)),
      tools_(std::move(tools)) {}

ToolBridgeClient::~ToolBridgeClient() {
  Shutdown();
}

const ToolDefinition* ToolBridgeClient::FindTool(const std::string& tool_id) const {
  for (const ToolDefinition& tool : tools_) {
    if (tool.id == tool_id) {
      return &tool;
    }
  }
  return nullptr;
}

bool ToolBridgeClient::CheckPolicy(const ToolInvocationRequest& request,
                                    std::string* denial_reason) const {
  const ToolDefinition* tool = FindTool(request.tool_id);
  if (tool == nullptr) {
    if (denial_reason != nullptr) {
      *denial_reason = "Tool '" + request.tool_id + "' is not registered in the tool registry.";
    }
    return false;
  }

  if (!tool->allowed_personas.empty() &&
      !VectorContains(tool->allowed_personas, request.persona_id)) {
    if (denial_reason != nullptr) {
      *denial_reason = "Tool '" + request.tool_id + "' is not allowed for persona '" +
                       request.persona_id + "'.";
    }
    return false;
  }

  if (VectorContains(tool->denied_personas, request.persona_id)) {
    if (denial_reason != nullptr) {
      *denial_reason = "Tool '" + request.tool_id + "' is explicitly denied for persona '" +
                       request.persona_id + "'.";
    }
    return false;
  }

  if (VectorContains(tool->denied_security_modes, request.security_mode_id)) {
    if (denial_reason != nullptr) {
      *denial_reason = "Tool '" + request.tool_id + "' is denied in security mode '" +
                       request.security_mode_id + "'.";
    }
    return false;
  }

  if (tool->requires_network && request.route_type == "direct") {
    if (request.security_mode_id == "ghost" || request.security_mode_id == "redteam") {
      if (denial_reason != nullptr) {
        *denial_reason = "Tool '" + request.tool_id +
                         "' requires network but the active route is direct (system ISP). "
                         "This is not permitted under the current security mode.";
      }
      return false;
    }
  }

  return true;
}

bool ToolBridgeClient::Invoke(const ToolInvocationRequest& request,
                               ToolInvocationResult* result,
                               std::string* error) {
  if (result == nullptr) {
    if (error != nullptr) {
      *error = "Tool invocation result output target was null.";
    }
    return false;
  }

  const std::string invocation_id =
      "inv-" + std::to_string(++invocation_counter_) + "-" + request.tool_id;
  result->invocation_id = invocation_id;
  result->tool_id = request.tool_id;

  std::string denial_reason;
  if (!CheckPolicy(request, &denial_reason)) {
    result->policy_denied = true;
    result->denial_reason = denial_reason;
    WriteAuditEvent("denied", request, *result);
    if (error != nullptr) {
      *error = denial_reason;
    }
    return false;
  }

  const ToolDefinition* tool = FindTool(request.tool_id);
  if (tool == nullptr) {
    if (error != nullptr) {
      *error = "Tool not found after policy check (internal error).";
    }
    return false;
  }

  WriteAuditEvent("invoked", request, *result);

  if (!Start(error)) {
    return false;
  }

  std::ostringstream command;
  command << "INVOKE\t" << SanitizeProtocolField(invocation_id)
          << "\t" << SanitizeProtocolField(tool->binary_path)
          << "\t" << tool->max_runtime_seconds
          << "\t1";

  for (const std::string& arg : request.args) {
    command << "\t" << SanitizeProtocolField(arg);
  }

  if (!SendLine(command.str(), error)) {
    return false;
  }

  bool saw_completed = false;

  while (true) {
    std::string line;
    if (!ReadLine(&line, error)) {
      return false;
    }

    const std::vector<std::string> fields = Split(line, '\t');
    if (fields.empty()) {
      continue;
    }

    const std::string& tag = fields[0];

    if (tag == "STARTED") {
      continue;
    }

    if (tag == "OUTPUT" && fields.size() >= 4) {
      result->output.push_back({fields[2], fields[3]});
      continue;
    }

    if (tag == "COMPLETED" && fields.size() >= 3) {
      try {
        result->exit_code = std::stoi(fields[2]);
      } catch (...) {
        result->exit_code = -1;
      }
      saw_completed = true;
      continue;
    }

    if (tag == "FAILED" && fields.size() >= 3) {
      result->timed_out = (fields[2] == "timeout");
      if (fields.size() >= 4) {
        result->failure_reason = fields[3];
      }
      continue;
    }

    if (tag == "OK") {
      break;
    }

    if (tag == "ERROR") {
      if (error != nullptr) {
        *error = fields.size() > 1 ? fields[1] : "Tool bridge returned an unspecified error.";
      }
      return false;
    }
  }

  if (tool->output_to_quarantine && !result->output.empty()) {
    std::string quarantine_path;
    std::string quarantine_error;
    if (!WriteOutputToQuarantine(request.quarantine_root, invocation_id,
                                  result->output, &quarantine_path, &quarantine_error)) {
      if (error != nullptr) {
        *error = "Failed to quarantine tool output: " + quarantine_error;
      }
      return false;
    }
    result->quarantine_path = quarantine_path;
  }

  WriteAuditEvent(saw_completed ? "completed" : "failed", request, *result);
  return true;
}

bool ToolBridgeClient::Start(std::string* error) {
  if (child_pid_ > 0) {
    return true;
  }

  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    if (error != nullptr) {
      *error = "Failed to create tool bridge pipes: " + std::string(std::strerror(errno));
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
      *error = "Failed to fork tool bridge: " + std::string(std::strerror(errno));
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
    execl(bridge_binary_path_.c_str(), bridge_binary_path_.c_str(), "--stdio", nullptr);
    _exit(127);
  }

  CloseFd(&stdin_pipe[0]);
  CloseFd(&stdout_pipe[1]);
  write_fd_ = stdin_pipe[1];
  read_fd_ = stdout_pipe[0];
  return true;
}

void ToolBridgeClient::Shutdown() {
  if (child_pid_ > 0) {
    std::string ignored;
    SendLine("SHUTDOWN", &ignored);
  }
  CloseFd(&write_fd_);
  CloseFd(&read_fd_);
  if (child_pid_ > 0) {
    int status = 0;
    waitpid(child_pid_, &status, 0);
    child_pid_ = -1;
  }
}

bool ToolBridgeClient::SendLine(const std::string& line, std::string* error) {
  if (write_fd_ < 0) {
    if (error != nullptr) {
      *error = "Tool bridge write pipe is not available.";
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
        *error = "Failed to write to tool bridge: " + std::string(std::strerror(errno));
      }
      return false;
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
  return true;
}

bool ToolBridgeClient::ReadLine(std::string* line, std::string* error) {
  if (read_fd_ < 0) {
    if (error != nullptr) {
      *error = "Tool bridge read pipe is not available.";
    }
    return false;
  }

  line->clear();
  char ch = '\0';
  while (true) {
    const ssize_t n = read(read_fd_, &ch, 1);
    if (n < 0) {
      if (error != nullptr) {
        *error = "Failed to read from tool bridge: " + std::string(std::strerror(errno));
      }
      return false;
    }
    if (n == 0) {
      if (error != nullptr) {
        *error = "Tool bridge closed unexpectedly.";
      }
      return false;
    }
    if (ch == '\n') {
      return true;
    }
    line->push_back(ch);
  }
}

bool ToolBridgeClient::WriteAuditEvent(const std::string& event_type,
                                        const ToolInvocationRequest& request,
                                        const ToolInvocationResult& result) const {
  if (request.artifact_event_log_path.empty()) {
    return true;
  }

  std::ofstream output(request.artifact_event_log_path, std::ios::out | std::ios::app);
  if (!output.is_open()) {
    return false;
  }

  output << "{"
         << "\"event_type\":\"tool:" << JsonEscape(event_type) << "\","
         << "\"invocation_id\":\"" << JsonEscape(result.invocation_id) << "\","
         << "\"tool_id\":\"" << JsonEscape(request.tool_id) << "\","
         << "\"persona_id\":\"" << JsonEscape(request.persona_id) << "\","
         << "\"security_mode\":\"" << JsonEscape(request.security_mode_id) << "\","
         << "\"route_type\":\"" << JsonEscape(request.route_type) << "\","
         << "\"timestamp_ms\":" << NowMillisString() << ",";

  if (event_type == "invoked") {
    output << "\"args\":[";
    for (std::size_t i = 0; i < request.args.size(); ++i) {
      if (i > 0) {
        output << ",";
      }
      output << "\"" << JsonEscape(request.args[i]) << "\"";
    }
    output << "]";
  } else if (event_type == "completed" || event_type == "failed") {
    output << "\"exit_code\":" << result.exit_code << ","
           << "\"timed_out\":" << (result.timed_out ? "true" : "false") << ","
           << "\"output_lines\":" << result.output.size();
    if (!result.failure_reason.empty()) {
      output << ",\"failure_reason\":\"" << JsonEscape(result.failure_reason) << "\"";
    }
    if (!result.quarantine_path.empty()) {
      output << ",\"quarantine_path\":\"" << JsonEscape(result.quarantine_path) << "\"";
    }
  } else if (event_type == "denied") {
    output << "\"reason\":\"" << JsonEscape(result.denial_reason) << "\"";
  } else {
    output << "\"detail\":\"" << JsonEscape(event_type) << "\"";
  }

  output << "}\n";
  return output.good();
}

bool ToolBridgeClient::WriteOutputToQuarantine(const std::string& quarantine_root,
                                                const std::string& invocation_id,
                                                const std::vector<ToolOutputLine>& output,
                                                std::string* quarantine_path,
                                                std::string* error) const {
  if (quarantine_root.empty()) {
    if (error != nullptr) {
      *error = "Quarantine root is not set for tool output.";
    }
    return false;
  }

  const std::filesystem::path tool_quarantine_dir =
      std::filesystem::path(quarantine_root) / ("tool-" + invocation_id);

  std::error_code directory_error;
  std::filesystem::create_directories(tool_quarantine_dir, directory_error);
  if (directory_error) {
    if (error != nullptr) {
      *error = "Failed to create tool quarantine directory: " + directory_error.message();
    }
    return false;
  }

  const std::filesystem::path output_file = tool_quarantine_dir / "output.txt";
  std::ofstream out(output_file, std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    if (error != nullptr) {
      *error = "Failed to open tool quarantine output file: " + output_file.string();
    }
    return false;
  }

  for (const ToolOutputLine& line : output) {
    out << "[" << line.stream << "] " << line.content << "\n";
  }

  if (!out.good()) {
    if (error != nullptr) {
      *error = "Failed to flush tool quarantine output: " + output_file.string();
    }
    return false;
  }

  if (quarantine_path != nullptr) {
    *quarantine_path = output_file.string();
  }
  return true;
}

}  // namespace veyra
