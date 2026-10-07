#include "veyra/runtime/secure_store.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using veyra::SecureStore;
namespace fs = std::filesystem;
void Check(bool value) { if (!value) throw std::runtime_error("assertion failed"); }
template<class F> void Reject(F fn) {
  bool failed = false;
  try { fn(); } catch (const std::exception&) { failed = true; }
  Check(failed);
}
std::string Read(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void Put(const std::string& path, const std::string& value) { std::ofstream(path, std::ios::binary) << value; }
int main() {
  char pattern[] = "/tmp/veyra-storage-test-XXXXXX";
  std::string root = mkdtemp(pattern);
  try {
    SecureStore::Keys keys{};
    for (size_t i = 0; i < keys.size(); ++i) keys[i] = static_cast<unsigned char>(i + 1);
    SecureStore store([&](bool) { return keys; });
    const std::string secret = "https://private.invalid\tuser\tpassword-never-on-disk\n";
    const std::string context = "persona:logins";
    auto a = SecureStore::Seal(keys, secret, context);
    auto b = SecureStore::Seal(keys, secret, context);
    Check(a != b && a.find("password-never-on-disk") == std::string::npos);
    Check(SecureStore::Open(keys, a, context) == secret);
    Check(SecureStore::Open(keys, SecureStore::Seal(keys, "", context), context).empty());
    Reject([&] { SecureStore::Open(keys, a, "other-persona:logins"); });
    for (size_t i = 0; i < a.size(); ++i) {
      auto corrupt = a; corrupt[i] ^= 1;
      Reject([&] { SecureStore::Open(keys, corrupt, context); });
    }
    for (size_t n = 0; n < a.size(); ++n)
      Reject([&] { SecureStore::Open(keys, a.substr(0, n), context); });
    auto wrong = keys; wrong[0] ^= 1;
    Reject([&] { SecureStore::Open(wrong, a, context); });
    wrong = keys; wrong[32] ^= 1;
    Reject([&] { SecureStore::Open(wrong, a, context); });
    const auto path = root + "/profile/logins.tsv";
    fs::create_directory(root + "/profile");
    Put(path, secret);
    Check(store.Read(path, context) == secret);
    Check(!fs::exists(path));
    Check(Read(path + ".vlt").find(secret) == std::string::npos);
    struct stat st{}; Check(stat((path + ".vlt").c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
    Check(store.Read(path, context) == secret); // Idempotent migration.
    Put(path, secret); // Interrupted migration: encrypted file already committed.
    Check(store.Read(path, context) == secret && !fs::exists(path));
    auto corrupt = Read(path + ".vlt"); corrupt.back() ^= 1;
    Put(path + ".vlt", corrupt); Put(path, "legacy copy");
    Reject([&] { store.Read(path, context); });
    Reject([&] { store.Write(path, context, "overwrite"); });
    Check(Read(path + ".vlt") == corrupt && Read(path) == "legacy copy");
    store.Remove(path);
    Put(path, secret);
    SecureStore unavailable([](bool) -> SecureStore::Keys { throw std::runtime_error("keyring unavailable"); });
    Reject([&] { unavailable.Read(path, context); });
    Check(Read(path) == secret && !fs::exists(path + ".vlt"));
    store.Read(path, context);
    Reject([&] { unavailable.Read(path, context); });
    Check(fs::exists(path + ".vlt"));
    store.Remove(path);
    fs::create_symlink(root + "/unrelated", path);
    Reject([&] { store.Read(path, context); });
    fs::remove(path);
    fs::create_symlink(root + "/unrelated", path + ".vlt");
    Reject([&] { store.Write(path, context, "unsafe"); });
    fs::remove(path + ".vlt");
    fs::create_directory_symlink(root + "/profile", root + "/linked");
    Reject([&] { store.Write(root + "/linked/file", context, "unsafe"); });
    // Cross-process updates must not lose records.
    store.Write(path, context, "");
    pid_t child = fork();
    Check(child >= 0);
    for (int n = 0; n < 12; ++n) store.Update(path, context, [](std::string& data) { data += "record\n"; });
    if (!child) _exit(0);
    int status = 0; waitpid(child, &status, 0);
    Check(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    Check(store.Read(path, context).size() == 24 * 7);
    store.Remove(path);
    Check(!fs::exists(path) && !fs::exists(path + ".vlt"));
    fs::remove_all(root);
    std::cout << "PASS: crypto authentication, randomness, both keys, context binding, migration, interruption, corruption, unavailable keys, permissions, symlinks, concurrent updates, deletion\n";
  } catch (...) { fs::remove_all(root); throw; }
}
