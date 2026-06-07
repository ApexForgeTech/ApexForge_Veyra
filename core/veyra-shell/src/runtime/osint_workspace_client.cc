#include "veyra/runtime/osint_workspace_client.h"

#include "veyra/serialization/json.h"

#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

namespace veyra {
namespace {

// Username/active probing can fan out to many third-party sites, so allow more
// headroom than a single AI call while still bounding a wedged child.
constexpr int kRequestTimeoutMs = 120000;

void CloseFd(int* fd) {
  if (fd != nullptr && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

std::string J(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (const char ch : value) {
    switch (ch) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
          out += buf;
        } else {
          out.push_back(ch);
        }
        break;
    }
  }
  out.push_back('"');
  return out;
}

std::string Jb(bool v) { return v ? "true" : "false"; }

// Minimal serializer for the attribute sub-objects we receive (string/number/
// bool leaves, nested objects/arrays). json.h provides no serializer.
std::string SerializeValue(const JsonValue& v) {
  if (v.IsString()) return J(v.AsString());
  if (v.IsBool()) return Jb(v.AsBool());
  if (v.IsNull()) return "null";
  if (v.IsNumber()) {
    std::ostringstream n;
    n << v.AsNumber();
    return n.str();
  }
  if (v.IsArray()) {
    std::string out = "[";
    bool first = true;
    for (const JsonValue& item : v.AsArray()) {
      if (!first) out += ",";
      out += SerializeValue(item);
      first = false;
    }
    return out + "]";
  }
  if (v.IsObject()) {
    std::string out = "{";
    bool first = true;
    for (const auto& kv : v.AsObject()) {
      if (!first) out += ",";
      out += J(kv.first) + ":" + SerializeValue(kv.second);
      first = false;
    }
    return out + "}";
  }
  return "null";
}

const JsonValue* Find(const JsonValue::Object& obj, const std::string& key) {
  const auto it = obj.find(key);
  return it == obj.end() ? nullptr : &it->second;
}

std::string Str(const JsonValue::Object& obj, const std::string& key) {
  const JsonValue* v = Find(obj, key);
  return (v != nullptr && v->IsString()) ? v->AsString() : std::string();
}

}  // namespace

OsintWorkspaceClient::OsintWorkspaceClient(std::string python_binary,
                                           std::string script_path,
                                           OsintPolicyDefinition policy)
    : python_binary_(std::move(python_binary)),
      script_path_(std::move(script_path)),
      policy_(std::move(policy)) {}

std::string OsintWorkspaceClient::BuildPolicyJson() const {
  std::ostringstream out;
  out << "{"
      << "\"enabled\":" << Jb(policy_.enabled) << ","
      << "\"require_route\":" << Jb(policy_.require_route) << ","
      << "\"allow_whois\":" << Jb(policy_.allow_whois) << ","
      << "\"allow_dns\":" << Jb(policy_.allow_dns) << ","
      << "\"allow_archive\":" << Jb(policy_.allow_archive) << ","
      << "\"allow_username_search\":" << Jb(policy_.allow_username_search) << ","
      << "\"allow_active_probing\":" << Jb(policy_.allow_active_probing) << ","
      << "\"max_targets_per_case\":" << policy_.max_targets_per_case << ","
      << "\"max_username_sites\":" << policy_.max_username_sites << ","
      << "\"retain_cases\":" << Jb(policy_.retain_cases)
      << "}";
  return out.str();
}

std::string OsintWorkspaceClient::BuildRouteJson(const OsintRouteContext& route) const {
  std::ostringstream out;
  out << "{"
      << "\"proxy_uri\":" << J(route.proxy_uri) << ","
      << "\"route_type\":" << J(route.route_type)
      << "}";
  return out.str();
}

bool OsintWorkspaceClient::RunMethod(const std::string& method,
                                     const std::string& target,
                                     const OsintRouteContext& route,
                                     OsintLookupResult* result,
                                     std::string* error) {
  std::ostringstream req;
  req << "{\"id\":\"q\",\"method\":" << J(method) << ",\"params\":{"
      << "\"target\":" << J(target) << ","
      << "\"route\":" << BuildRouteJson(route) << ","
      << "\"policy\":" << BuildPolicyJson()
      << "}}";

  std::string response;
  if (!RunRequest(req.str(), &response, error)) {
    return false;
  }

  const JsonParseResult parsed = ParseJson(response);
  if (!parsed.error.empty() || !parsed.value.IsObject()) {
    if (error != nullptr) *error = "OSINT workspace returned malformed JSON.";
    return false;
  }
  const JsonValue::Object& obj = parsed.value.AsObject();

  const JsonValue* ok = Find(obj, "ok");
  result->ok = ok != nullptr && ok->IsBool() && ok->AsBool();
  if (!result->ok) {
    result->error = Str(obj, "error");
    if (result->error.empty()) result->error = "unknown OSINT error";
    // The Python side reports policy/leak refusals as ok=false; surface that.
    if (result->error.find("Leak prevention") != std::string::npos ||
        result->error.find("not permitted") != std::string::npos ||
        result->error.find("disabled") != std::string::npos) {
      result->policy_denied = true;
    }
    if (error != nullptr) *error = result->error;
    return false;
  }

  const JsonValue* res = Find(obj, "result");
  if (res == nullptr || !res->IsObject()) {
    return true;
  }
  const JsonValue::Object& r = res->AsObject();

  result->summary = Str(r, "summary");
  result->egress = Str(r, "egress");
  if (const JsonValue* okr = Find(r, "ok_route"); okr != nullptr && okr->IsBool()) {
    result->ok_route = okr->AsBool();
  }

  if (const JsonValue* ents = Find(r, "entities"); ents != nullptr && ents->IsArray()) {
    for (const JsonValue& item : ents->AsArray()) {
      if (!item.IsObject()) continue;
      const JsonValue::Object& e = item.AsObject();
      OsintEntity entity;
      entity.type = Str(e, "type");
      entity.value = Str(e, "value");
      entity.source = Str(e, "source");
      if (const JsonValue* attr = Find(e, "attributes"); attr != nullptr && attr->IsObject()) {
        entity.attributes_json = SerializeValue(*attr);
      }
      result->entities.push_back(std::move(entity));
    }
  }

  if (const JsonValue* rels = Find(r, "relationships"); rels != nullptr && rels->IsArray()) {
    for (const JsonValue& item : rels->AsArray()) {
      if (!item.IsObject()) continue;
      const JsonValue::Object& rl = item.AsObject();
      result->relationships.push_back({Str(rl, "from"), Str(rl, "to"), Str(rl, "kind")});
    }
  }

  if (const JsonValue* tl = Find(r, "timeline"); tl != nullptr && tl->IsArray()) {
    for (const JsonValue& item : tl->AsArray()) {
      if (!item.IsObject()) continue;
      const JsonValue::Object& t = item.AsObject();
      OsintTimelineEvent ev;
      if (const JsonValue* ts = Find(t, "ts_ms"); ts != nullptr && ts->IsNumber()) {
        ev.ts_ms = static_cast<std::int64_t>(ts->AsNumber());
      }
      ev.event = Str(t, "event");
      ev.detail = Str(t, "detail");
      result->timeline.push_back(std::move(ev));
    }
  }

  return true;
}

bool OsintWorkspaceClient::Ping(const OsintRouteContext& route,
                                OsintLookupResult* result, std::string* error) {
  return RunMethod("ping", "", route, result, error);
}

bool OsintWorkspaceClient::Whois(const std::string& target, const OsintRouteContext& route,
                                 OsintLookupResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_whois) {
    result->policy_denied = true;
    result->error = "WHOIS is disabled for this persona's OSINT policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  return RunMethod("whois_lookup", target, route, result, error);
}

bool OsintWorkspaceClient::Dns(const std::string& target, const OsintRouteContext& route,
                               OsintLookupResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_dns) {
    result->policy_denied = true;
    result->error = "DNS lookup is disabled for this persona's OSINT policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  return RunMethod("dns_lookup", target, route, result, error);
}

bool OsintWorkspaceClient::Archive(const std::string& target, const OsintRouteContext& route,
                                   OsintLookupResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_archive) {
    result->policy_denied = true;
    result->error = "Archive lookup is disabled for this persona's OSINT policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  return RunMethod("archive_lookup", target, route, result, error);
}

bool OsintWorkspaceClient::UsernameSearch(const std::string& target,
                                          const OsintRouteContext& route,
                                          OsintLookupResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_username_search || policy_.max_username_sites <= 0) {
    result->policy_denied = true;
    result->error = "Username search is disabled for this persona's OSINT policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  return RunMethod("username_search", target, route, result, error);
}

bool OsintWorkspaceClient::RunRequest(const std::string& request_json,
                                      std::string* response_json,
                                      std::string* error) {
  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    if (error != nullptr) {
      *error = "Failed to create OSINT workspace pipes: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]); CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]); CloseFd(&stdout_pipe[1]);
    return false;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    if (error != nullptr) {
      *error = "Failed to fork OSINT workspace: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]); CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]); CloseFd(&stdout_pipe[1]);
    return false;
  }

  if (pid == 0) {
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    CloseFd(&stdin_pipe[0]); CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]); CloseFd(&stdout_pipe[1]);
    execlp(python_binary_.c_str(), python_binary_.c_str(), script_path_.c_str(),
           "--stdio", static_cast<char*>(nullptr));
    _exit(127);
  }

  CloseFd(&stdin_pipe[0]);
  CloseFd(&stdout_pipe[1]);

  struct sigaction ignore_action;
  std::memset(&ignore_action, 0, sizeof(ignore_action));
  ignore_action.sa_handler = SIG_IGN;
  struct sigaction previous_action;
  sigaction(SIGPIPE, &ignore_action, &previous_action);

  auto abort_child = [&](const std::string& message) -> bool {
    if (error != nullptr) *error = message;
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    sigaction(SIGPIPE, &previous_action, nullptr);
    return false;
  };

  const std::string payload = request_json + "\n{\"id\":\"x\",\"method\":\"shutdown\"}\n";
  const char* cursor = payload.c_str();
  std::size_t remaining = payload.size();
  while (remaining > 0) {
    struct pollfd pfd;
    pfd.fd = stdin_pipe[1];
    pfd.events = POLLOUT;
    pfd.revents = 0;
    const int ready = poll(&pfd, 1, kRequestTimeoutMs);
    if (ready < 0) {
      if (errno == EINTR) continue;
      return abort_child("Poll on OSINT workspace stdin failed: " +
                         std::string(std::strerror(errno)));
    }
    if (ready == 0) {
      return abort_child("OSINT workspace timed out accepting the request.");
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return abort_child("OSINT workspace closed its input before the request was sent.");
    }
    const ssize_t written = write(stdin_pipe[1], cursor, remaining);
    if (written < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      return abort_child("Failed to write to OSINT workspace: " +
                         std::string(std::strerror(errno)));
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
  CloseFd(&stdin_pipe[1]);

  std::string first_line;
  bool got_line = false;
  char buffer[1024];
  while (!got_line) {
    struct pollfd pfd;
    pfd.fd = stdout_pipe[0];
    pfd.events = POLLIN;
    pfd.revents = 0;
    const int ready = poll(&pfd, 1, kRequestTimeoutMs);
    if (ready < 0) {
      if (errno == EINTR) continue;
      return abort_child("Poll on OSINT workspace stdout failed: " +
                         std::string(std::strerror(errno)));
    }
    if (ready == 0) {
      return abort_child("OSINT workspace timed out producing a response.");
    }
    const ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      return abort_child("Failed to read from OSINT workspace: " +
                         std::string(std::strerror(errno)));
    }
    if (n == 0) {
      break;
    }
    for (ssize_t i = 0; i < n; ++i) {
      if (buffer[i] == '\n') {
        got_line = true;
        break;
      }
      first_line.push_back(buffer[i]);
    }
  }

  CloseFd(&stdout_pipe[0]);
  kill(pid, SIGKILL);
  int status = 0;
  waitpid(pid, &status, 0);
  sigaction(SIGPIPE, &previous_action, nullptr);

  if (!got_line || first_line.empty()) {
    if (error != nullptr) *error = "OSINT workspace produced no response.";
    return false;
  }
  if (response_json != nullptr) *response_json = first_line;
  return true;
}

}  // namespace veyra
