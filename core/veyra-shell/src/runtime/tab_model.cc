#include "veyra/runtime/tab_model.h"

#include <utility>

namespace veyra {

TabModel::TabModel(std::string id, std::string profile_id, std::string initial_url, std::string title)
    : id_(std::move(id)),
      profile_id_(std::move(profile_id)),
      current_url_(std::move(initial_url)),
      title_(std::move(title)),
      history_{current_url_},
      history_index_(0) {}

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

void TabModel::Navigate(std::string url, std::string title) {
  current_url_ = std::move(url);
  title_ = std::move(title);

  if (history_index_ + 1 < history_.size()) {
    history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_index_ + 1), history_.end());
  }

  history_.push_back(current_url_);
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
