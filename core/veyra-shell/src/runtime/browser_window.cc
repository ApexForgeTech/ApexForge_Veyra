#include "veyra/runtime/browser_window.h"

#include "veyra/runtime/route_service.h"

#include <utility>

namespace veyra {
namespace {

std::string MakeTabTitle(const RuntimeProfile& profile, const std::string& title) {
  return title + " [" + profile.persona.display_name + "]";
}

}  // namespace

BrowserWindow::BrowserWindow(std::string id, std::string title)
    : id_(std::move(id)), title_(std::move(title)) {}

BrowserWindow BrowserWindow::CreatePrimaryWindow(const RuntimeProfile& profile,
                                                 const RouteRuntimeState& route_state,
                                                 std::string initial_url) {
  BrowserWindow window("window:primary", "Veyra Primary Window");
  window.OpenTab(profile, route_state, std::move(initial_url), MakeTabTitle(profile, "Home"));
  return window;
}

const std::string& BrowserWindow::id() const {
  return id_;
}

const std::string& BrowserWindow::title() const {
  return title_;
}

const std::vector<TabModel>& BrowserWindow::tabs() const {
  return tabs_;
}

const TabModel* BrowserWindow::ActiveTab() const {
  if (tabs_.empty() || active_tab_index_ >= tabs_.size()) {
    return nullptr;
  }
  return &tabs_[active_tab_index_];
}

TabModel* BrowserWindow::ActiveTabMutable() {
  if (tabs_.empty() || active_tab_index_ >= tabs_.size()) {
    return nullptr;
  }
  return &tabs_[active_tab_index_];
}

const std::string* BrowserWindow::ActiveTabId() const {
  const TabModel* tab = ActiveTab();
  if (tab == nullptr) {
    return nullptr;
  }
  return &tab->id();
}

TabModel& BrowserWindow::OpenTab(const RuntimeProfile& profile,
                                 const RouteRuntimeState& route_state,
                                 std::string initial_url,
                                 std::string title) {
  const std::string tab_id = "tab:" + std::to_string(tabs_.size() + 1);
  tabs_.emplace_back(tab_id, profile.persona.id, std::move(initial_url), std::move(title),
                     route_state.route_profile_id, route_state.route_type,
                     route_state.health_status, route_state.proxy_uri,
                     route_state.dns_resolver,
                     profile.runtime_policy.history_enabled,
                     profile.runtime_policy.max_history_entries);
  active_tab_index_ = tabs_.size() - 1;
  return tabs_.back();
}

bool BrowserWindow::ActivateTab(std::size_t index) {
  if (index >= tabs_.size()) {
    return false;
  }
  active_tab_index_ = index;
  return true;
}

bool BrowserWindow::ActivateTabById(const std::string& tab_id) {
  for (std::size_t index = 0; index < tabs_.size(); ++index) {
    if (tabs_[index].id() == tab_id) {
      return ActivateTab(index);
    }
  }
  return false;
}

bool BrowserWindow::SyncTabFromEngine(const std::string& tab_id, std::string url, std::string title) {
  TabModel* tab = FindTabById(tab_id);
  if (tab == nullptr) {
    return false;
  }

  tab->SyncFromEngine(std::move(url), std::move(title));
  return true;
}

bool BrowserWindow::ApplyRouteStateToProfileTabs(const std::string& profile_id,
                                                 const RouteRuntimeState& route_state) {
  bool updated = false;
  for (TabModel& tab : tabs_) {
    if (tab.profile_id() == profile_id) {
      tab.ApplyRouteState(route_state);
      updated = true;
    }
  }
  return updated;
}

TabModel* BrowserWindow::FindTabById(const std::string& tab_id) {
  for (TabModel& tab : tabs_) {
    if (tab.id() == tab_id) {
      return &tab;
    }
  }
  return nullptr;
}

}  // namespace veyra
