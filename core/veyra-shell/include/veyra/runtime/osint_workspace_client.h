#ifndef VEYRA_RUNTIME_OSINT_WORKSPACE_CLIENT_H_
#define VEYRA_RUNTIME_OSINT_WORKSPACE_CLIENT_H_

#include "veyra/models/foundation_models.h"

#include <cstdint>
#include <string>
#include <vector>

namespace veyra {

struct OsintEntity {
  std::string type;
  std::string value;
  std::string source;
  std::string attributes_json;  // raw JSON object of extra attributes
};

struct OsintRelationship {
  std::string from;
  std::string to;
  std::string kind;
};

struct OsintTimelineEvent {
  std::int64_t ts_ms = 0;
  std::string event;
  std::string detail;
};

struct OsintLookupResult {
  bool ok = false;
  bool policy_denied = false;
  std::string error;
  std::string summary;
  std::vector<OsintEntity> entities;
  std::vector<OsintRelationship> relationships;
  std::vector<OsintTimelineEvent> timeline;
  // Populated by Ping:
  bool ok_route = false;
  std::string egress;  // "proxy" | "direct" | "blocked"
};

// Active route context passed to the workspace so every lookup is forced through
// the persona's proxy. proxy_uri is empty for a direct route.
struct OsintRouteContext {
  std::string proxy_uri;
  std::string route_type;
};

// Talks to the Python OSINT workspace (agents/osint-workspace) over JSONL stdio.
// Per-call child process (started, queried once, killed) so a hung lookup can
// never freeze the shell or leave a zombie. Uses the same SIGPIPE-safe, poll-
// bounded IO as the AI orchestrator client.
class OsintWorkspaceClient {
 public:
  OsintWorkspaceClient(std::string python_binary, std::string script_path,
                       OsintPolicyDefinition policy);

  bool Ping(const OsintRouteContext& route, OsintLookupResult* result, std::string* error);
  bool Whois(const std::string& target, const OsintRouteContext& route,
             OsintLookupResult* result, std::string* error);
  bool Dns(const std::string& target, const OsintRouteContext& route,
           OsintLookupResult* result, std::string* error);
  bool Archive(const std::string& target, const OsintRouteContext& route,
               OsintLookupResult* result, std::string* error);
  bool UsernameSearch(const std::string& target, const OsintRouteContext& route,
                      OsintLookupResult* result, std::string* error);

  const OsintPolicyDefinition& policy() const { return policy_; }

 private:
  bool RunMethod(const std::string& method, const std::string& target,
                 const OsintRouteContext& route, OsintLookupResult* result,
                 std::string* error);
  bool RunRequest(const std::string& request_json, std::string* response_json,
                  std::string* error);
  std::string BuildPolicyJson() const;
  std::string BuildRouteJson(const OsintRouteContext& route) const;

  std::string python_binary_;
  std::string script_path_;
  OsintPolicyDefinition policy_;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_OSINT_WORKSPACE_CLIENT_H_
