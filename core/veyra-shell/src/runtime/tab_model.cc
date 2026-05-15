#include "veyra/runtime/tab_model.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace veyra {

TabModel::TabModel(std::string id,
                   std::string profile_id,
                   std::string initial_url,
                   std::string title,
                   bool history_enabled,
                   std::size_t max_history_entries)
    : id_(std::move(id)),
      profile_id_(std::move(profile_id)),
      current_url_(std::move(initial_url)),
      title_(std::move(title)),
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
