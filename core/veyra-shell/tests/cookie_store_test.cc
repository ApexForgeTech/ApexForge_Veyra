// Include the adapter to exercise the exact serialization used by WebKit.
// Link with section GC so unrelated browser/UI code is discarded.
#include "../src/engine/webkitgtk_browser_engine.cc"
#include <cassert>
int main() {
  GList* list = nullptr;
  auto* c = soup_cookie_new("session_token", "private-cookie", "example.test", "/account", 3600);
  soup_cookie_set_secure(c, TRUE);
  soup_cookie_set_http_only(c, TRUE);
  soup_cookie_set_same_site_policy(c, SOUP_SAME_SITE_POLICY_STRICT);
  list = g_list_append(list, c);
  list = g_list_append(list, soup_cookie_new("domain", "yes", ".example.test", "/", 3600));
  list = g_list_append(list, soup_cookie_new("session", "must-not-persist", "example.test", "/", -1));
  auto serialized = veyra::SerializeCookies(list);
  assert(serialized.find("must-not-persist") == std::string::npos);
  GList* restored = veyra::DeserializeCookies(serialized);
  assert(g_list_length(restored) == 2);
  for (auto* p = restored; p; p = p->next) {
    auto* cookie = static_cast<SoupCookie*>(p->data);
    if (std::string(soup_cookie_get_name(cookie)) == "session_token") {
      assert(std::string(soup_cookie_get_value(cookie)) == "private-cookie");
      assert(std::string(soup_cookie_get_domain(cookie)) == "example.test");
      assert(std::string(soup_cookie_get_path(cookie)) == "/account");
      assert(soup_cookie_get_secure(cookie) && soup_cookie_get_http_only(cookie));
      assert(soup_cookie_get_same_site_policy(cookie) == SOUP_SAME_SITE_POLICY_STRICT);
    } else assert(std::string(soup_cookie_get_domain(cookie)) == ".example.test");
  }
  g_list_free_full(list, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
  g_list_free_full(restored, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
  assert(veyra::DeserializeCookies("") == nullptr);
  bool rejected = false;
  try { veyra::DeserializeCookies("broken"); } catch (...) { rejected = true; }
  assert(rejected);
  // Upgrade reset removes only website state, leaves downloads and encrypted stores.
  char root[] = "/tmp/veyra-cookie-test-XXXXXX";
  auto directory = std::filesystem::path(mkdtemp(root));
  for (auto name : {"storage", "cache", "cookies", "downloads"}) {
    std::filesystem::create_directory(directory / name);
    std::ofstream(directory / name / "old-data") << "legacy-data";
  }
  std::ofstream(directory / "cookie-jar.vlt") << "encrypted";
  veyra::PurgeLegacyWebsiteState(directory);
  assert(!std::filesystem::exists(directory / "cookies"));
  assert(!std::filesystem::exists(directory / "storage"));
  assert(!std::filesystem::exists(directory / "cache"));
  assert(std::filesystem::exists(directory / "downloads/old-data"));
  assert(std::filesystem::exists(directory / "cookie-jar.vlt"));
  veyra::PurgeLegacyWebsiteState(directory); // repeated upgrade is harmless
  std::filesystem::remove_all(directory);
  std::cout << "PASS: cookie persistence preserves attributes and host/domain scope, drops session cookies, validates format; upgrade cleanup is bounded and repeatable\n";
}
