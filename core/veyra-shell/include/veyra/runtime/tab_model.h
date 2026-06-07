#ifndef VEYRA_RUNTIME_TAB_MODEL_H_
#define VEYRA_RUNTIME_TAB_MODEL_H_

#include <cstddef>
#include <string>
#include <vector>

namespace veyra {

struct RouteRuntimeState;

class TabModel {
 public:
  TabModel(std::string id,
           std::string profile_id,
           std::string initial_url,
           std::string title,
           std::string route_profile_id,
           std::string route_type,
           std::string route_health_status,
           std::string route_proxy_uri,
           std::string route_dns_resolver,
           bool history_enabled,
           std::size_t max_history_entries);

  const std::string& id() const;
  const std::string& profile_id() const;
  const std::string& current_url() const;
  const std::string& title() const;
  const std::string& route_profile_id() const;
  const std::string& route_type() const;
  const std::string& route_health_status() const;
  const std::string& route_proxy_uri() const;
  const std::string& route_dns_resolver() const;
  const std::vector<std::string>& history() const;
  bool history_enabled() const;
  std::size_t max_history_entries() const;

  void Navigate(std::string url, std::string title);
  void SyncFromEngine(std::string url, std::string title);
  void ApplyRouteState(const RouteRuntimeState& route_state);
  bool CanGoBack() const;
  bool CanGoForward() const;
  bool GoBack();
  bool GoForward();

 private:
  std::string id_;
  std::string profile_id_;
  std::string current_url_;
  std::string title_;
  std::string route_profile_id_;
  std::string route_type_;
  std::string route_health_status_;
  std::string route_proxy_uri_;
  std::string route_dns_resolver_;
  std::vector<std::string> history_;
  std::size_t history_index_ = 0;
  bool history_enabled_ = true;
  std::size_t max_history_entries_ = 1;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_TAB_MODEL_H_
