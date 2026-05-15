#include "veyra/runtime/browser_window.h"

#include <utility>

namespace veyra {
namespace {

std::string MakeTabTitle(const RuntimeProfile& profile, const std::string& title) {
  return title + " [" + profile.persona.display_name + "]";
}

}  // namespace

BrowserWindow::BrowserWindow(std::string id, std::string title)
    : id_(std::move(id)), title_(std::move(title)) {}

BrowserWindow BrowserWindow::CreatePrimaryWindow(const RuntimeProfile& profile) {
  BrowserWindow window("window:primary", "Veyra Primary Window");
  window.OpenTab(profile, "about:home", MakeTabTitle(profile, "Home"));
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

TabModel& BrowserWindow::OpenTab(const RuntimeProfile& profile, std::string initial_url, std::string title) {
  const std::string tab_id = "tab:" + std::to_string(tabs_.size() + 1);
  tabs_.emplace_back(tab_id, profile.persona.id, std::move(initial_url), std::move(title),
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

}  // namespace veyra
