#ifndef VEYRA_RUNTIME_TOOL_BRIDGE_CLIENT_H_
#define VEYRA_RUNTIME_TOOL_BRIDGE_CLIENT_H_

#include "veyra/models/foundation_models.h"
#include "veyra/runtime/profile_manager.h"

#include <cstdint>
#include <string>
#include <vector>

namespace veyra {

struct ToolInvocationRequest {
  std::string tool_id;
  std::vector<std::string> args;
  std::string persona_id;
  std::string security_mode_id;
  std::string route_type;
  std::string quarantine_root;
  std::string artifact_event_log_path;
};

struct ToolOutputLine {
  std::string stream;
  std::string content;
};

struct ToolInvocationResult {
  std::string invocation_id;
  std::string tool_id;
  int exit_code = -1;
  bool timed_out = false;
  bool policy_denied = false;
  std::string denial_reason;   // set only when policy_denied=true
  std::string failure_reason;  // set when timed_out=true or spawn failed
  std::vector<ToolOutputLine> output;
  std::string quarantine_path;
  std::string audit_event_path;
};

class ToolBridgeClient {
 public:
  explicit ToolBridgeClient(std::string bridge_binary_path,
                            std::vector<ToolDefinition> tools);
  ~ToolBridgeClient();

  bool Invoke(const ToolInvocationRequest& request,
              ToolInvocationResult* result,
              std::string* error);

  const ToolDefinition* FindTool(const std::string& tool_id) const;
  bool CheckPolicy(const ToolInvocationRequest& request,
                   std::string* denial_reason) const;

 private:
  bool Start(std::string* error);
  void Shutdown();
  bool SendLine(const std::string& line, std::string* error);
  bool ReadLine(std::string* line, std::string* error);

  bool WriteAuditEvent(const std::string& event_type,
                       const ToolInvocationRequest& request,
                       const ToolInvocationResult& result) const;
  bool WriteOutputToQuarantine(const std::string& quarantine_root,
                               const std::string& invocation_id,
                               const std::vector<ToolOutputLine>& output,
                               std::string* quarantine_path,
                               std::string* error) const;

  std::string bridge_binary_path_;
  std::vector<ToolDefinition> tools_;
  int write_fd_ = -1;
  int read_fd_ = -1;
  pid_t child_pid_ = -1;
  std::uint64_t invocation_counter_ = 0;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_TOOL_BRIDGE_CLIENT_H_
