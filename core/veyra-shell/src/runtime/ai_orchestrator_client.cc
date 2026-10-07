#include "veyra/runtime/ai_orchestrator_client.h"

#include "veyra/serialization/json.h"

#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace veyra {
namespace {

// Hard ceiling on a single AI request. Python's own urllib timeout (60s) should
// trip first; this is defense-in-depth so a wedged child can never freeze the
// GTK main loop forever.
constexpr int kRequestTimeoutMs = 90000;

void CloseFd(int* fd) {
  if (fd != nullptr && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

std::string JsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 2);
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
  return out;
}

const JsonValue* Find(const JsonValue::Object& obj, const std::string& key) {
  const auto it = obj.find(key);
  return it == obj.end() ? nullptr : &it->second;
}

}  // namespace

AiOrchestratorClient::AiOrchestratorClient(std::string python_binary,
                                           std::string script_path,
                                           AiPolicyDefinition policy)
    : python_binary_(std::move(python_binary)),
      script_path_(std::move(script_path)),
      policy_(std::move(policy)) {}

bool AiOrchestratorClient::RunRequest(const std::string& request_json,
                                      std::string* response_json,
                                      std::string* error) {
  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    if (error != nullptr) {
      *error = "Failed to create AI orchestrator pipes: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]); CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]); CloseFd(&stdout_pipe[1]);
    return false;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    if (error != nullptr) {
      *error = "Failed to fork AI orchestrator: " + std::string(std::strerror(errno));
    }
    CloseFd(&stdin_pipe[0]); CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]); CloseFd(&stdout_pipe[1]);
    return false;
  }

  if (pid == 0) {
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    // Keep child stderr attached to the shell's stderr for diagnostics.
    CloseFd(&stdin_pipe[0]); CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]); CloseFd(&stdout_pipe[1]);
    execlp(python_binary_.c_str(), python_binary_.c_str(), script_path_.c_str(),
           "--stdio", static_cast<char*>(nullptr));
    _exit(127);
  }

  CloseFd(&stdin_pipe[0]);
  CloseFd(&stdout_pipe[1]);

  // A child that exec-fails or dies early turns our writes into broken-pipe
  // writes. The default SIGPIPE action would terminate the whole browser shell,
  // so ignore it for the duration of this exchange and restore afterwards.
  struct sigaction ignore_action;
  std::memset(&ignore_action, 0, sizeof(ignore_action));
  ignore_action.sa_handler = SIG_IGN;
  struct sigaction previous_action;
  sigaction(SIGPIPE, &ignore_action, &previous_action);

  // Helper: forcibly tear down the child and restore SIGPIPE on any failure.
  auto abort_child = [&](const std::string& message) -> bool {
    if (error != nullptr) {
      *error = message;
    }
    CloseFd(&stdin_pipe[1]);
    CloseFd(&stdout_pipe[0]);
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    sigaction(SIGPIPE, &previous_action, nullptr);
    return false;
  };

  // Write the request line, then a shutdown line, then close stdin. Poll the
  // write fd so a child blocked on inference can never deadlock our writes.
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
      return abort_child("Poll on AI orchestrator stdin failed: " +
                         std::string(std::strerror(errno)));
    }
    if (ready == 0) {
      return abort_child("AI orchestrator timed out accepting the request.");
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return abort_child("AI orchestrator closed its input before the request was sent.");
    }
    const ssize_t written = write(stdin_pipe[1], cursor, remaining);
    if (written < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      return abort_child("Failed to write to AI orchestrator: " +
                         std::string(std::strerror(errno)));
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
  CloseFd(&stdin_pipe[1]);

  // Read the first response line (the answer to our request), bounded by a poll
  // timeout so a wedged model never hangs the shell.
  std::string first_line;
  bool got_line = false;
  char buffer[512];
  while (!got_line) {
    struct pollfd pfd;
    pfd.fd = stdout_pipe[0];
    pfd.events = POLLIN;
    pfd.revents = 0;
    const int ready = poll(&pfd, 1, kRequestTimeoutMs);
    if (ready < 0) {
      if (errno == EINTR) continue;
      return abort_child("Poll on AI orchestrator stdout failed: " +
                         std::string(std::strerror(errno)));
    }
    if (ready == 0) {
      return abort_child("AI orchestrator timed out producing a response.");
    }
    const ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      return abort_child("Failed to read from AI orchestrator: " +
                         std::string(std::strerror(errno)));
    }
    if (n == 0) {
      break;  // EOF before a full line.
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
  // We only ever consume one response line; terminate the child so a wedged
  // process can never block waitpid (and to avoid a broken-pipe write in the
  // child as it tries to answer the trailing shutdown line).
  kill(pid, SIGKILL);
  int status = 0;
  waitpid(pid, &status, 0);
  sigaction(SIGPIPE, &previous_action, nullptr);

  if (!got_line || first_line.empty()) {
    if (error != nullptr) {
      *error = "AI orchestrator produced no response.";
    }
    return false;
  }

  if (response_json != nullptr) {
    *response_json = first_line;
  }
  return true;
}

bool AiOrchestratorClient::ParseResponse(const std::string& response_json,
                                         AiRequestResult* result,
                                         std::string* error) {
  const JsonParseResult parsed = ParseJson(response_json);
  if (!parsed.error.empty() || !parsed.value.IsObject()) {
    if (error != nullptr) {
      *error = "AI orchestrator returned malformed JSON.";
    }
    return false;
  }

  const JsonValue::Object& obj = parsed.value.AsObject();

  const JsonValue* ok = Find(obj, "ok");
  result->ok = ok != nullptr && ok->IsBool() && ok->AsBool();
  if (!result->ok) {
    const JsonValue* err = Find(obj, "error");
    result->error = (err != nullptr && err->IsString()) ? err->AsString()
                                                        : "unknown orchestrator error";
    if (error != nullptr) {
      *error = result->error;
    }
    return false;
  }

  const JsonValue* res = Find(obj, "result");
  if (res == nullptr || !res->IsObject()) {
    return true;  // ok with no structured result
  }
  const JsonValue::Object& r = res->AsObject();

  auto str = [&](const std::string& k) -> std::string {
    const JsonValue* v = Find(r, k);
    return (v != nullptr && v->IsString()) ? v->AsString() : std::string();
  };

  result->summary = str("summary");
  result->explanation = str("explanation");
  result->risk_level = str("risk_level");
  result->source = str("source");

  if (const JsonValue* score = Find(r, "score"); score != nullptr && score->IsNumber()) {
    result->risk_score = static_cast<int>(score->AsNumber());
  }
  if (const JsonValue* online = Find(r, "model_online");
      online != nullptr && online->IsBool()) {
    result->model_online = online->AsBool();
  }

  return true;
}

bool AiOrchestratorClient::Ping(AiRequestResult* result, std::string* error) {
  const std::string request =
      "{\"id\":\"ping\",\"method\":\"ping\",\"params\":{\"endpoint\":\"" +
      JsonEscape(policy_.endpoint) + "\",\"model\":\"" + JsonEscape(policy_.model) + "\"}}";
  std::string response;
  if (!RunRequest(request, &response, error)) {
    return false;
  }
  return ParseResponse(response, result, error);
}

bool AiOrchestratorClient::Summarize(const std::string& page_text,
                                     AiRequestResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_page_content) {
    result->policy_denied = true;
    result->error = "AI page-content analysis is disabled for this persona's AI policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  const std::string request =
      "{\"id\":\"sum\",\"method\":\"summarize\",\"params\":{\"text\":\"" +
      JsonEscape(page_text) + "\",\"model\":\"" + JsonEscape(policy_.model) +
      "\",\"endpoint\":\"" + JsonEscape(policy_.endpoint) +
      "\",\"max_input_chars\":" + std::to_string(policy_.max_input_chars) + "}}";
  std::string response;
  if (!RunRequest(request, &response, error)) {
    return false;
  }
  return ParseResponse(response, result, error);
}

bool AiOrchestratorClient::PhishingCheck(const std::string& url, const std::string& title,
                                         AiRequestResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_phishing_check) {
    result->policy_denied = true;
    result->error = "AI phishing check is disabled for this persona's AI policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  const std::string request =
      "{\"id\":\"phish\",\"method\":\"phishing_check\",\"params\":{\"url\":\"" +
      JsonEscape(url) + "\",\"title\":\"" + JsonEscape(title) +
      "\",\"model\":\"" + JsonEscape(policy_.model) +
      "\",\"endpoint\":\"" + JsonEscape(policy_.endpoint) +
      "\",\"allow_model\":" + (policy_.allow_page_content ? "true" : "false") + "}}";
  std::string response;
  if (!RunRequest(request, &response, error)) {
    return false;
  }
  return ParseResponse(response, result, error);
}

bool AiOrchestratorClient::ExplainScript(const std::string& code,
                                         AiRequestResult* result, std::string* error) {
  if (!policy_.enabled || !policy_.allow_script_analysis) {
    result->policy_denied = true;
    result->error = "AI script analysis is disabled for this persona's AI policy.";
    if (error != nullptr) *error = result->error;
    return false;
  }
  const std::string request =
      "{\"id\":\"script\",\"method\":\"explain_script\",\"params\":{\"code\":\"" +
      JsonEscape(code) + "\",\"model\":\"" + JsonEscape(policy_.model) +
      "\",\"endpoint\":\"" + JsonEscape(policy_.endpoint) +
      "\",\"max_input_chars\":" + std::to_string(policy_.max_input_chars) + "}}";
  std::string response;
  if (!RunRequest(request, &response, error)) {
    return false;
  }
  return ParseResponse(response, result, error);
}

}  // namespace veyra
