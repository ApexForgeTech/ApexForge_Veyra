#include "veyra/runtime/tab_model.h"

#include "veyra/runtime/route_service.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace veyra {

TabModel::TabModel(std::string id,
                   std::string profile_id,
                   std::string initial_url,
                   std::string title,
                   std::string route_profile_id,
                   std::string route_type,
                   std::string route_health_status,
                   std::string route_proxy_uri,
                   std::string route_dns_resolver,
                   bool history_enabled,
                   std::size_t max_history_entries)
    : id_(std::move(id)),
      profile_id_(std::move(profile_id)),
      current_url_(std::move(initial_url)),
      title_(std::move(title)),
      route_profile_id_(std::move(route_profile_id)),
      route_type_(std::move(route_type)),
      route_health_status_(std::move(route_health_status)),
      route_proxy_uri_(std::move(route_proxy_uri)),
      route_dns_resolver_(std::move(route_dns_resolver)),
      history_{current_url_},
      history_index_(0),
      history_enabled_(history_enabled),
      max_history_entries_(std::max<std::size_t>(1, max_history_entries)) {}

const std::string& TabModel::id() const {
  return id_;
}

const std::string& TabModel::profile_id() const {
  return profile_id_;
}

const std::string& TabModel::current_url() const {
  return current_url_;
}

const std::string& TabModel::title() const {
  return title_;
}

const std::string& TabModel::route_profile_id() const {
  return route_profile_id_;
}

const std::string& TabModel::route_type() const {
  return route_type_;
}

const std::string& TabModel::route_health_status() const {
  return route_health_status_;
}

const std::string& TabModel::route_proxy_uri() const {
  return route_proxy_uri_;
}

const std::string& TabModel::route_dns_resolver() const {
  return route_dns_resolver_;
}

const std::vector<std::string>& TabModel::history() const {
  return history_;
}

bool TabModel::history_enabled() const {
  return history_enabled_;
}

std::size_t TabModel::max_history_entries() const {
  return max_history_entries_;
}

void TabModel::Navigate(std::string url, std::string title) {
  if (url == current_url_) {
    title_ = std::move(title);
    return;
  }

  current_url_ = std::move(url);
  title_ = std::move(title);

  if (!history_enabled_) {
    history_.assign(1, current_url_);
    history_index_ = 0;
    return;
  }

  if (history_index_ + 1 < history_.size()) {
    history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_index_ + 1), history_.end());
  }

  history_.push_back(current_url_);

  if (history_.size() > max_history_entries_) {
    const std::size_t overflow = history_.size() - max_history_entries_;
    history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(overflow));
  }

  history_index_ = history_.size() - 1;
}

void TabModel::SyncFromEngine(std::string url, std::string title) {
  Navigate(std::move(url), std::move(title));
}

void TabModel::ApplyRouteState(const RouteRuntimeState& route_state) {
  route_profile_id_ = route_state.route_profile_id;
  route_type_ = route_state.route_type;
  route_health_status_ = route_state.health_status;
  route_proxy_uri_ = route_state.proxy_uri;
  route_dns_resolver_ = route_state.dns_resolver;
}

bool TabModel::CanGoBack() const {
  return history_index_ > 0;
}

bool TabModel::CanGoForward() const {
  return history_index_ + 1 < history_.size();
}

bool TabModel::GoBack() {
  if (!CanGoBack()) {
    return false;
  }

  --history_index_;
  current_url_ = history_[history_index_];
  return true;
}

bool TabModel::GoForward() {
  if (!CanGoForward()) {
    return false;
  }

  ++history_index_;
  current_url_ = history_[history_index_];
  return true;
}

}  // namespace veyra
