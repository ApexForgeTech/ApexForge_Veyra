#include "veyra/runtime/secure_store.h"

#include <libsecret/secret.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <glib/gstdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace veyra {
namespace {
constexpr size_t kLimit = 64 * 1024 * 1024;
const std::string kMagic = "VEYRA-VLT2\n";
[[noreturn]] void Fail(const char* message) { throw std::runtime_error(message); }
struct Fd {
  int fd;
  explicit Fd(int value) : fd(value) { if (fd < 0) Fail("Secure storage: cannot open private file"); }
  ~Fd() { close(fd); }
};
void PrivateParent(const std::string& path) {
  auto parent = std::filesystem::path(path).parent_path();
  // Never follow symlinked directory components for secret storage.
  std::filesystem::path current;
  for (const auto& part : parent) {
    current /= part;
    struct stat st{};
    if (lstat(current.c_str(), &st) == 0) {
      if (!S_ISDIR(st.st_mode)) Fail("Secure storage: unsafe directory");
    } else if (errno != ENOENT || mkdir(current.c_str(), 0700) != 0) {
      Fail("Secure storage: cannot create directory");
    }
  }
  struct stat st{};
  if (lstat(parent.c_str(), &st) || st.st_uid != geteuid() || chmod(parent.c_str(), 0700))
    Fail("Secure storage: directory ownership or permissions invalid");
}
struct Lock {
  Fd file;
  explicit Lock(const std::string& path) : file((PrivateParent(path),
      open((path + ".lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600))) {
    struct stat st{};
    if (fstat(file.fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        fchmod(file.fd, 0600) || flock(file.fd, LOCK_EX)) Fail("Secure storage: lock failed");
  }
};
bool ReadFile(const std::string& path, std::string& out) {
  out.clear();
  int raw = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (raw < 0 && errno == ENOENT) return false;
  Fd fd(raw);
  struct stat st{};
  if (fstat(fd.fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
      st.st_nlink != 1 || st.st_size < 0 || size_t(st.st_size) > kLimit + 1024)
    Fail("Secure storage: unsafe or oversized file");
  if (fchmod(fd.fd, 0600)) Fail("Secure storage: cannot restrict file permissions");
  char buffer[8192];
  for (;;) {
    ssize_t n = read(fd.fd, buffer, sizeof buffer);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) Fail("Secure storage: read failed");
    if (!n) break;
    out.append(buffer, size_t(n));
    if (out.size() > kLimit + 1024) Fail("Secure storage: file too large");
  }
  return true;
}
void SyncParent(const std::string& path) {
  Fd dir(open(std::filesystem::path(path).parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (fsync(dir.fd)) Fail("Secure storage: directory sync failed");
}
void AtomicWrite(const std::string& path, const std::string& data) {
  std::string pattern = path + ".tmp-XXXXXX";
  std::vector<char> name(pattern.begin(), pattern.end()); name.push_back(0);
  Fd fd(mkostemp(name.data(), O_CLOEXEC));
  try {
    if (fchmod(fd.fd, 0600)) Fail("Secure storage: file permissions failed");
    size_t pos = 0;
    while (pos < data.size()) {
      ssize_t n = write(fd.fd, data.data() + pos, data.size() - pos);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) Fail("Secure storage: write failed");
      pos += size_t(n);
    }
    if (fsync(fd.fd) || rename(name.data(), path.c_str())) Fail("Secure storage: commit failed");
    SyncParent(path);
  } catch (...) { unlink(name.data()); throw; }
}
void RemoveLegacy(const std::string& path) {
  if (unlink(path.c_str()) && errno != ENOENT) Fail("Secure storage: legacy cleanup failed");
  SyncParent(path);
}
SecureStore::Keys SystemKeys(bool create) {
  const std::string root = std::string(g_get_user_data_dir()) + "/veyra/keys/master";
  Lock lock(root);
  static const SecretSchema schema = [] {
    SecretSchema value{};
    value.name = "com.apexforge.veyra.Storage.v2";
    value.flags = SECRET_SCHEMA_NONE;
    value.attributes[0] = {"purpose", SECRET_SCHEMA_ATTRIBUTE_STRING};
    return value;
  }();
  GError* error = nullptr;
  gchar* stored = secret_password_lookup_sync(&schema, nullptr, &error, "purpose", "storage", nullptr);
  if (error) { g_error_free(error); Fail("Secure storage: OS keyring unavailable or locked"); }
  SecureStore::Keys keys{};
  if (stored) {
    gsize length = 0;
    guchar* bytes = g_base64_decode(stored, &length);
    secret_password_free(stored);
    if (length != keys.size()) { g_free(bytes); Fail("Secure storage: invalid OS keyring key"); }
    std::memcpy(keys.data(), bytes, length);
    OPENSSL_cleanse(bytes, length); g_free(bytes);
    struct stat marker{};
    if (lstat((root + ".initialized").c_str(), &marker) != 0)
      AtomicWrite(root + ".initialized", "Veyra key initialized\n");
    return keys;
  }
  struct stat marker{};
  if (!create || lstat((root + ".initialized").c_str(), &marker) == 0)
    Fail("Secure storage: encryption key missing; refusing to replace it");
  if (RAND_bytes(keys.data(), keys.size()) != 1) Fail("Secure storage: random generator failed");
  gchar* encoded = g_base64_encode(keys.data(), keys.size());
  const bool ok = secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT,
      "Veyra encrypted storage keys", encoded, nullptr, &error, "purpose", "storage", nullptr);
  OPENSSL_cleanse(encoded, strlen(encoded)); g_free(encoded);
  if (error) g_error_free(error);
  if (!ok) { OPENSSL_cleanse(keys.data(), keys.size()); Fail("Secure storage: cannot persist key in OS keyring"); }
  AtomicWrite(root + ".initialized", "Veyra key initialized\n");
  return keys;
}
struct WipeKeys {
  SecureStore::Keys& keys;
  ~WipeKeys() { OPENSSL_cleanse(keys.data(), keys.size()); }
};
std::string Crypt(bool encrypt, const unsigned char* key, const std::string& input,
                  const std::string& aad) {
  if (input.size() > kLimit + 128 || aad.size() > 4096) Fail("Secure storage: size limit exceeded");
  if (!encrypt && input.size() < 28) Fail("Secure storage: truncated envelope");
  unsigned char nonce[12];
  if (encrypt) {
    if (RAND_bytes(nonce, sizeof nonce) != 1) Fail("Secure storage: random generator failed");
  } else std::memcpy(nonce, input.data(), sizeof nonce);
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx || EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key, nonce, encrypt) != 1)
    Fail("Secure storage: cipher init failed");
  int n = 0;
  if (EVP_CipherUpdate(ctx.get(), nullptr, &n, reinterpret_cast<const unsigned char*>(aad.data()), aad.size()) != 1)
    Fail("Secure storage: context authentication failed");
  const size_t length = encrypt ? input.size() : input.size() - 28;
  std::string output(length + 16, '\0');
  const auto* bytes = reinterpret_cast<const unsigned char*>(input.data()) + (encrypt ? 0 : 12);
  if (EVP_CipherUpdate(ctx.get(), reinterpret_cast<unsigned char*>(output.data()), &n, bytes, length) != 1)
    Fail("Secure storage: cipher operation failed");
  int total = n;
  if (!encrypt && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
      const_cast<char*>(input.data() + input.size() - 16)) != 1) Fail("Secure storage: tag rejected");
  if (EVP_CipherFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(output.data()) + total, &n) != 1) {
    OPENSSL_cleanse(output.data(), output.size()); Fail("Secure storage: authentication failed");
  }
  output.resize(total + n);
  if (encrypt) {
    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, sizeof tag, tag) != 1)
      Fail("Secure storage: tag generation failed");
    output.insert(0, reinterpret_cast<char*>(nonce), sizeof nonce);
    output.append(reinterpret_cast<char*>(tag), sizeof tag);
  }
  return output;
}
std::string LoadLocked(const std::string& path, const std::string& context,
                       const SecureStore::KeyProvider& provider) {
  std::string data;
  if (ReadFile(path + ".vlt", data)) {
    auto keys = provider(false); WipeKeys wipe{keys};
    std::string plain = SecureStore::Open(keys, data, context);
    // Retry cleanup after an interrupted migration, but only after authentication.
    RemoveLegacy(path);
    return plain;
  }
  if (!ReadFile(path, data)) return {};
  auto keys = provider(true); WipeKeys wipe{keys};
  AtomicWrite(path + ".vlt", SecureStore::Seal(keys, data, context));
  std::string check;
  if (!ReadFile(path + ".vlt", check) || SecureStore::Open(keys, check, context) != data)
    Fail("Secure storage: migration verification failed");
  RemoveLegacy(path);
  return data;
}
}  // namespace
SecureStore::SecureStore(KeyProvider provider) : keys_(provider ? std::move(provider) : SystemKeys) {}
std::string SecureStore::Seal(const Keys& keys, const std::string& plain, const std::string& context) {
  if (plain.size() > kLimit) Fail("Secure storage: plaintext too large");
  auto inner = Crypt(true, keys.data(), plain, kMagic + "inner:" + context);
  return kMagic + Crypt(true, keys.data() + 32, inner, kMagic + "outer:" + context);
}
std::string SecureStore::Open(const Keys& keys, const std::string& envelope, const std::string& context) {
  if (envelope.compare(0, kMagic.size(), kMagic) != 0) Fail("Secure storage: unsupported envelope");
  auto inner = Crypt(false, keys.data() + 32, envelope.substr(kMagic.size()), kMagic + "outer:" + context);
  return Crypt(false, keys.data(), inner, kMagic + "inner:" + context);
}
std::string SecureStore::Read(const std::string& path, const std::string& context) {
  Lock lock(path); return LoadLocked(path, context, keys_);
}
void SecureStore::Update(const std::string& path, const std::string& context,
                         const std::function<void(std::string&)>& change) {
  Lock lock(path);
  auto data = LoadLocked(path, context, keys_);
  change(data);
  struct stat st{};
  bool exists = lstat((path + ".vlt").c_str(), &st) == 0;
  auto keys = keys_(!exists); WipeKeys wipe{keys};
  AtomicWrite(path + ".vlt", Seal(keys, data, context));
}
void SecureStore::Write(const std::string& path, const std::string& context, const std::string& value) {
  Update(path, context, [&](std::string& data) { data = value; });
}
void SecureStore::Remove(const std::string& path) {
  Lock lock(path);
  RemoveLegacy(path);
  RemoveLegacy(path + ".vlt");
}
}  // namespace veyra
