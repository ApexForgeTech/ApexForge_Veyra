#ifndef VEYRA_RUNTIME_ROUTER_SUPERVISOR_H_
#define VEYRA_RUNTIME_ROUTER_SUPERVISOR_H_

#include <string>

namespace veyra {

// Manages the local anonymity routers (Tor SOCKS proxy on 9050, I2P HTTP proxy
// on 4444) the way Tor Browser manages its bundled tor: Veyra launches and owns
// the process with its own data dir + config, so the user does not need a
// system-installed/configured router. If a router is already listening on its
// port (e.g. a system service), Veyra reuses it instead of spawning another.
//
// Binaries are discovered on PATH or via explicit paths (env VEYRA_TOR_BIN /
// VEYRA_I2PD_BIN, or SetTorBinary/SetI2pBinary). For a fully self-contained
// build, ship the binaries under <runtime_root>/routers and point here.
class RouterSupervisor {
 public:
  explicit RouterSupervisor(std::string runtime_root);
  ~RouterSupervisor();

  RouterSupervisor(const RouterSupervisor&) = delete;
  RouterSupervisor& operator=(const RouterSupervisor&) = delete;

  // Ensures the router needed for `route_type` ("tor", "i2p", "chained") is
  // up. Reuses an already-listening router; otherwise spawns one. No-op for
  // direct/vpn. Returns true if the proxy port is (or becomes) reachable.
  // `status` receives a human-readable line for the dashboard/logs.
  bool Ensure(const std::string& route_type, std::string* status);

  void SetTorBinary(const std::string& path);
  void SetI2pBinary(const std::string& path);

  // Terminates any routers Veyra started (not ones it merely reused).
  void StopAll();

 private:
  bool EnsureTor(std::string* status);
  bool EnsureI2p(std::string* status);

  std::string runtime_root_;
  std::string tor_binary_;
  std::string i2pd_binary_;
  long tor_pid_ = 0;   // GPid we spawned (0 = none / reused external)
  long i2pd_pid_ = 0;
};

}  // namespace veyra

#endif  // VEYRA_RUNTIME_ROUTER_SUPERVISOR_H_
