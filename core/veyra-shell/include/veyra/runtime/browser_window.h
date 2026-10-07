#ifndef VEYRA_RUNTIME_BROWSER_WINDOW_H_
#define VEYRA_RUNTIME_BROWSER_WINDOW_H_

#include "veyra/runtime/profile_manager.h"
#include "veyra/runtime/tab_model.h"

#include <cstddef>
#include <string>
#include <vector>

namespace veyra {

struct RouteRuntimeState;

class BrowserWindow {
 public:
  BrowserWindow(std::string id, std::string title);

  static BrowserWindow CreatePrimaryWindow(const RuntimeProfile& profile,
                                           const RouteRuntimeState& route_state,
                                           std::string initial_url = "https://example.org");

  const std::string& id() const;
  const std::string& title() const;
  const std::vector<TabModel>& tabs() const;
  const TabModel* ActiveTab() const;
  TabModel* ActiveTabMutable();
  const std::string* ActiveTabId() const;

  TabModel& OpenTab(const RuntimeProfile& profile,
                    const RouteRuntimeState& route_state,
                    std::string initial_url,
                    std::string title);
  bool ActivateTab(std::size_t index);
  bool ActivateTabById(const std::string& tab_id);
  bool SyncTabFromEngine(const std::string& tab_id, std::string url, std::string title);
  bool ApplyRouteStateToProfileTabs(const std::string& profile_id, const RouteRuntimeState& route_state);

 private:
  TabModel* FindTabById(const std::string& tab_id);

  std::string id_;
  std::string title_;
  std::vector<TabModel> tabs_;
  std::size_t active_tab_index_ = 0;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_BROWSER_WINDOW_H_
