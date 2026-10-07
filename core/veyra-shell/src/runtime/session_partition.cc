#include "veyra/runtime/session_partition.h"

#include <utility>

namespace veyra {

SessionPartition::SessionPartition(std::string id,
                                   std::string storage_partition,
                                   std::string persona_id,
                                   bool is_persistent)
    : id_(std::move(id)),
      storage_partition_(std::move(storage_partition)),
      persona_id_(std::move(persona_id)),
      is_persistent_(is_persistent) {}

SessionPartition SessionPartition::FromPersona(const PersonaDefinition& persona) {
  const bool persistent = !persona.ephemeral;
  const std::string runtime_id = persistent ? "persist:" + persona.id : "memory:" + persona.id;
  const std::string storage_partition =
      persistent ? persona.storage_partition : "memory:" + persona.id;

  return SessionPartition(runtime_id, storage_partition, persona.id, persistent);
}

const std::string& SessionPartition::id() const {
  return id_;
}

const std::string& SessionPartition::storage_partition() const {
  return storage_partition_;
}

const std::string& SessionPartition::persona_id() const {
  return persona_id_;
}

bool SessionPartition::is_persistent() const {
  return is_persistent_;
}

}  // namespace veyra
