#ifndef VEYRA_CONFIG_FOUNDATION_LOADER_H_
#define VEYRA_CONFIG_FOUNDATION_LOADER_H_

#include "veyra/config/startup_config.h"
#include "veyra/models/foundation_models.h"

#include <vector>

namespace veyra {

struct LoadResult {
  FoundationState state;
  std::vector<ValidationIssue> issues;
};

LoadResult LoadFoundation(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_CONFIG_FOUNDATION_LOADER_H_
