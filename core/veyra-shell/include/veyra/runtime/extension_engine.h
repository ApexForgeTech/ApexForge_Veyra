#ifndef VEYRA_RUNTIME_EXTENSION_ENGINE_H_
#define VEYRA_RUNTIME_EXTENSION_ENGINE_H_

#include "veyra/models/foundation_models.h"

#include <string>
#include <vector>

namespace veyra {

struct ExtensionPolicy {
  std::string policy_id;
  std::string policy_display_name;

  bool allow_eval = true;
  bool allow_third_party_frames = true;
  bool block_mixed_content = false;
  bool allow_external_fonts = true;

  std::vector<std::string> blocked_domains;

  // Pre-built JavaScript injection: blocks eval() and Function() when allow_eval=false.
  // Empty when allow_eval=true.
  std::string eval_block_script;
};

ExtensionPolicy BuildExtensionPolicy(const ExtensionPolicyDefinition& definition);

std::string BuildEvalBlockScript();

bool IsBlockedDomain(const std::vector<std::string>& blocked_domains,
                     const std::string& host);

}  // namespace veyra

#endif  // VEYRA_RUNTIME_EXTENSION_ENGINE_H_
