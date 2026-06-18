#include "veyra/runtime/router_supervisor.h"

#include <glib.h>

#include <chrono>
#include <cstdlib>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

namespace veyra {
namespace {

// True if something is already accepting connections on 127.0.0.1:<port>.
bool PortOpen(int port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  // Non-blocking connect with a short timeout.
  ::fcntl(fd, F_SETFL, O_NONBLOCK);
  const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  bool open = (rc == 0);
  if (rc < 0) {
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    timeval tv{0, 200000};  // 200ms
    if (::select(fd + 1, nullptr, &wset, nullptr, &tv) > 0) {
      int err = 0;
      socklen_t len = sizeof(err);
      ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
      open = (err == 0);
    }
  }
  ::close(fd);
  return open;
}

// Waits up to `timeout_ms` for a port to open, polling every 250ms.
bool WaitForPort(int port, int timeout_ms) {
  const int steps = timeout_ms / 250;
  for (int i = 0; i < steps; ++i) {
    if (PortOpen(port)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  return PortOpen(port);
}

// Spawns argv detached. When lib_dir is non-empty it is prepended to
// LD_LIBRARY_PATH so a bundled router finds its own shipped .so libraries
// (the Tor-Browser model — no system libs required).
bool SpawnDetached(char** argv, const std::string& lib_dir, long* out_pid) {
  gchar** envp = g_get_environ();
  if (!lib_dir.empty()) {
    const gchar* existing = g_environ_getenv(envp, "LD_LIBRARY_PATH");
    std::string val = lib_dir;
    if (existing != nullptr && *existing != '\0') val += ":" + std::string(existing);
    envp = g_environ_setenv(envp, "LD_LIBRARY_PATH", val.c_str(), TRUE);
  }
  GPid pid = 0;
  GError* error = nullptr;
  const gboolean ok = g_spawn_async(
      nullptr, argv, envp,
      static_cast<GSpawnFlags>(G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL |
                               G_SPAWN_STDERR_TO_DEV_NULL),
      nullptr, nullptr, &pid, &error);
  g_strfreev(envp);
  if (!ok) {
    if (error != nullptr) g_error_free(error);
    return false;
  }
  *out_pid = static_cast<long>(pid);
  return true;
}

std::string ParentDir(const std::string& path) {
  const std::size_t slash = path.rfind('/');
  return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

}  // namespace

namespace {
// Directory of the running executable (for bundled-binary lookup).
std::string ExeDir() {
  char buf[4096];
  const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  std::string path(buf, n);
  const std::size_t slash = path.rfind('/');
  return slash == std::string::npos ? "." : path.substr(0, slash);
}

// Resolves a router binary, preferring binaries SHIPPED WITH Veyra (so it works
// with no system install — the Tor-Browser model) before falling back to PATH.
//   1. explicit env override
//   2. <exe_dir>/routers/<name>            (installed alongside the shell)
//   3. <exe_dir>/../vendor/routers/linux-x86_64/<name>  (repo/dev tree)
//   4. system PATH
std::string ResolveRouterBinary(const char* name, const char* env_var) {
  if (const char* e = std::getenv(env_var)) {
    if (*e != '\0' && g_file_test(e, G_FILE_TEST_IS_EXECUTABLE)) return e;
  }
  // Walk up from the executable dir; at each level try the installed layout
  // (<base>/routers/<name>[/<name>]) and the dev/vendor layout
  // (<base>/vendor/routers/linux-x86_64/<name>[/<name>]). The Tor expert bundle
  // nests the binary as <name>/<name>; single binaries (i2pd) sit directly.
  std::string base = ExeDir();
  for (int level = 0; level < 5; ++level) {
    const std::string roots[] = {
        base + "/routers", base + "/vendor/routers/linux-x86_64"};
    for (const std::string& root : roots) {
      const std::string candidates[] = {
          root + "/" + name + "/" + name,  // expert-bundle nesting (tor/tor)
          root + "/" + name,               // single binary
      };
      for (const std::string& c : candidates) {
        if (g_file_test(c.c_str(), G_FILE_TEST_IS_EXECUTABLE)) return c;
      }
    }
    base += "/..";
  }
  gchar* p = g_find_program_in_path(name);
  if (p != nullptr) { std::string r(p); g_free(p); return r; }
  return {};
}
}  // namespace

RouterSupervisor::RouterSupervisor(std::string runtime_root)
    : runtime_root_(std::move(runtime_root)) {
  tor_binary_ = ResolveRouterBinary("tor", "VEYRA_TOR_BIN");
  i2pd_binary_ = ResolveRouterBinary("i2pd", "VEYRA_I2PD_BIN");
}

RouterSupervisor::~RouterSupervisor() { StopAll(); }

void RouterSupervisor::SetTorBinary(const std::string& path) { tor_binary_ = path; }
void RouterSupervisor::SetI2pBinary(const std::string& path) { i2pd_binary_ = path; }

bool RouterSupervisor::Ensure(const std::string& route_type, std::string* status) {
  if (route_type == "tor" || route_type == "chained") {
    return EnsureTor(status);
  }
  if (route_type == "i2p") {
    return EnsureI2p(status);
  }
  if (status != nullptr) *status = "no router needed for " + route_type;
  return true;
}

bool RouterSupervisor::EnsureTor(std::string* status) {
  if (PortOpen(9050)) {
    if (status != nullptr) *status = "Tor reachable on 127.0.0.1:9050";
    return true;
  }
  if (tor_pid_ != 0) {  // we started it, still bootstrapping
    const bool up = WaitForPort(9050, 30000);
    if (status != nullptr) *status = up ? "Tor bootstrapped (managed)" : "Tor still bootstrapping";
    return up;
  }
  if (tor_binary_.empty()) {
    if (status != nullptr) *status = "Tor binary not found — install tor or bundle it";
    return false;
  }
  const std::string data_dir = runtime_root_ + "/routers/tor";
  g_mkdir_with_parents(data_dir.c_str(), 0700);
  const std::string lib_dir = ParentDir(tor_binary_);  // bundled libs live here
  // Bundled GeoIP (shipped next to the binary in ../data of the expert bundle).
  const std::string geoip = lib_dir + "/../data/geoip";
  const std::string geoip6 = lib_dir + "/../data/geoip6";
  const bool have_geoip = g_file_test(geoip.c_str(), G_FILE_TEST_EXISTS) != FALSE;
  // Veyra-owned tor instance: its own SocksPort + data dir, no control port.
  std::vector<char*> argv = {
      const_cast<char*>(tor_binary_.c_str()),
      const_cast<char*>("--SocksPort"), const_cast<char*>("127.0.0.1:9050"),
      const_cast<char*>("--DataDirectory"), const_cast<char*>(data_dir.c_str()),
      const_cast<char*>("--ControlPort"), const_cast<char*>("0"),
      const_cast<char*>("--AvoidDiskWrites"), const_cast<char*>("1")};
  if (have_geoip) {
    argv.push_back(const_cast<char*>("--GeoIPFile"));
    argv.push_back(const_cast<char*>(geoip.c_str()));
    argv.push_back(const_cast<char*>("--GeoIPv6File"));
    argv.push_back(const_cast<char*>(geoip6.c_str()));
  }
  argv.push_back(nullptr);
  if (!SpawnDetached(argv.data(), lib_dir, &tor_pid_)) {
    if (status != nullptr) *status = "Failed to launch tor";
    return false;
  }
  const bool up = WaitForPort(9050, 30000);
  if (status != nullptr) {
    *status = up ? "Tor launched + bootstrapped (Veyra-managed)"
                 : "Tor launched, bootstrapping…";
  }
  return up;
}

bool RouterSupervisor::EnsureI2p(std::string* status) {
  if (PortOpen(4444)) {
    if (status != nullptr) *status = "I2P reachable on 127.0.0.1:4444";
    return true;
  }
  if (i2pd_pid_ != 0) {
    const bool up = WaitForPort(4444, 60000);
    if (status != nullptr) *status = up ? "I2P bootstrapped (managed)" : "I2P still bootstrapping";
    return up;
  }
  if (i2pd_binary_.empty()) {
    if (status != nullptr) {
      *status = "i2pd not found — install i2pd or bundle it (apt install i2pd)";
    }
    return false;
  }
  const std::string data_dir = runtime_root_ + "/routers/i2pd";
  g_mkdir_with_parents(data_dir.c_str(), 0700);
  const std::string datadir_arg = "--datadir=" + data_dir;
  // i2pd enables the HTTP proxy on 4444 by default.
  char* argv[] = {const_cast<char*>(i2pd_binary_.c_str()),
                  const_cast<char*>(datadir_arg.c_str()),
                  const_cast<char*>("--httpproxy.address=127.0.0.1"),
                  const_cast<char*>("--httpproxy.port=4444"),
                  nullptr};
  if (!SpawnDetached(argv, ParentDir(i2pd_binary_), &i2pd_pid_)) {
    if (status != nullptr) *status = "Failed to launch i2pd";
    return false;
  }
  // I2P bootstrap (reseed + tunnels) takes longer than Tor.
  const bool up = WaitForPort(4444, 60000);
  if (status != nullptr) {
    *status = up ? "I2P launched + proxy ready (Veyra-managed)" : "I2P launched, bootstrapping…";
  }
  return up;
}

void RouterSupervisor::StopAll() {
  for (long* pid : {&tor_pid_, &i2pd_pid_}) {
    if (*pid != 0) {
      ::kill(static_cast<pid_t>(*pid), SIGTERM);
      g_spawn_close_pid(static_cast<GPid>(*pid));
      *pid = 0;
    }
  }
}

}  // namespace veyra
