#pragma once

#include <array>
#include <functional>
#include <string>

namespace veyra {
// Two independent AES-256-GCM layers. Keys live in the OS Secret Service,
// never alongside the ciphertext. All failures throw; no plaintext fallback.
class SecureStore {
 public:
  using Keys = std::array<unsigned char, 64>;
  using KeyProvider = std::function<Keys(bool create)>;
  explicit SecureStore(KeyProvider provider = {});
  // The legacy path is migrated to <path>.vlt, verified, then unlinked.
  std::string Read(const std::string& legacy_path, const std::string& context);
  void Update(const std::string& legacy_path, const std::string& context,
              const std::function<void(std::string&)>& change);
  void Write(const std::string& legacy_path, const std::string& context,
             const std::string& value);
  void Remove(const std::string& legacy_path);
  static std::string Seal(const Keys&, const std::string&, const std::string& context);
  static std::string Open(const Keys&, const std::string&, const std::string& context);
 private:
  KeyProvider keys_;
};
}  // namespace veyra
