#ifndef VEYRA_RUNTIME_ROUTE_SERVICE_H_
#define VEYRA_RUNTIME_ROUTE_SERVICE_H_

#include "veyra/config/startup_config.h"
#include "veyra/runtime/profile_manager.h"

#include <sys/types.h>
#include <string>
#include <vector>

namespace veyra {

struct RouteRuntimeState {
  std::string persona_id;
  std::string persona_name;
  std::string route_profile_id;
  std::string route_profile_name;
  std::string route_type;
  std::string dns_policy;
  std::string webrtc_policy;
  std::string leak_prevention_level;
  std::string routing_requirement;
  std::string health_status;
  std::string proxy_uri;
  std::string dns_resolver;
  std::string leak_status;
  std::string diagnostic_summary;
  std::vector<std::string> hops;
};

class RouteServiceClient {
 public:
  explicit RouteServiceClient(std::string binary_path);
  ~RouteServiceClient();

  bool Bootstrap(const std::vector<RuntimeProfile>& profiles,
                 const std::string& runtime_root,
                 std::string* error);
  bool SwitchRoute(const RuntimeProfile& profile,
                   const std::string& runtime_root,
                   RouteRuntimeState* updated_route,
                   std::string* error);
  bool RequestStatus(const std::string& persona_id,
                     RouteRuntimeState* route,
                     std::string* error);
  const RouteRuntimeState* FindByPersonaId(const std::string& persona_id) const;
  const std::vector<RouteRuntimeState>& routes() const;
  std::vector<ValidationIssue> WriteRouteStateReport(const std::string& report_path) const;

 private:
  bool Start(std::string* error);
  void Shutdown();
  bool SendProfileLine(const std::string& command,
                       const RuntimeProfile& profile,
                       std::string* error);
  bool SendLine(const std::string& line, std::string* error);
  bool ReadLine(std::string* line, std::string* error);
  bool ReadRouteResponse(RouteRuntimeState* route,
                         bool expect_ok,
                         std::string* error);
  void ReplaceOrAppendRoute(RouteRuntimeState route);

  std::string binary_path_;
  int write_fd_ = -1;
  int read_fd_ = -1;
  pid_t child_pid_ = -1;
  std::vector<RouteRuntimeState> routes_;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_ROUTE_SERVICE_H_
