#ifndef VEYRA_RUNTIME_AI_ORCHESTRATOR_CLIENT_H_
#define VEYRA_RUNTIME_AI_ORCHESTRATOR_CLIENT_H_

#include "veyra/models/foundation_models.h"

#include <sys/types.h>
#include <cstdint>
#include <string>

namespace veyra {

struct AiRequestResult {
  bool ok = false;
  bool policy_denied = false;
  std::string error;
  // Populated depending on method:
  std::string summary;       // summarize
  std::string explanation;   // explain_script / phishing detail
  std::string risk_level;    // phishing_check
  int risk_score = 0;        // phishing_check
  std::string source;        // "model" | "offline" | "heuristic"
  bool model_online = false; // ping
};

// Talks to the Python AI orchestrator (agents/ai-orchestrator) over JSONL stdio.
// The orchestrator is a per-call child process (started, queried once, shut down)
// so a hung model never leaves a zombie attached to the shell.
class AiOrchestratorClient {
 public:
  AiOrchestratorClient(std::string python_binary, std::string script_path,
                       AiPolicyDefinition policy);

  bool Ping(AiRequestResult* result, std::string* error);
  bool Summarize(const std::string& page_text, AiRequestResult* result, std::string* error);
  bool PhishingCheck(const std::string& url, const std::string& title,
                     AiRequestResult* result, std::string* error);
  bool ExplainScript(const std::string& code, AiRequestResult* result, std::string* error);

  const AiPolicyDefinition& policy() const { return policy_; }

 private:
  bool RunRequest(const std::string& request_json, std::string* response_json,
                  std::string* error);
  bool ParseResponse(const std::string& response_json, AiRequestResult* result,
                     std::string* error);

  std::string python_binary_;
  std::string script_path_;
  AiPolicyDefinition policy_;
  std::uint64_t request_counter_ = 0;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_AI_ORCHESTRATOR_CLIENT_H_
