#ifndef VEYRA_CONFIG_SCHEMA_VALIDATOR_H_
#define VEYRA_CONFIG_SCHEMA_VALIDATOR_H_

#include "veyra/config/startup_config.h"

#include <vector>

namespace veyra {

std::vector<ValidationIssue> ValidateSeedDataAgainstSchemas(const StartupConfig& config);

}  // namespace veyra

#endif  // VEYRA_CONFIG_SCHEMA_VALIDATOR_H_
