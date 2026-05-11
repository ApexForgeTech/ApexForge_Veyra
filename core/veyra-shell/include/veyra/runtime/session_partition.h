#ifndef VEYRA_RUNTIME_SESSION_PARTITION_H_
#define VEYRA_RUNTIME_SESSION_PARTITION_H_

#include "veyra/models/foundation_models.h"

#include <string>

namespace veyra {

class SessionPartition {
 public:
  static SessionPartition FromPersona(const PersonaDefinition& persona);

  const std::string& id() const;
  const std::string& storage_partition() const;
  const std::string& persona_id() const;
  bool is_persistent() const;

 private:
  SessionPartition(std::string id,
                   std::string storage_partition,
                   std::string persona_id,
                   bool is_persistent);

  std::string id_;
  std::string storage_partition_;
  std::string persona_id_;
  bool is_persistent_ = false;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_SESSION_PARTITION_H_
