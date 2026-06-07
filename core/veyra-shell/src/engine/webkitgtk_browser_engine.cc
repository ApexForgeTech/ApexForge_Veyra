#include "veyra/engine/webkitgtk_browser_engine.h"

#include "veyra/runtime/artifact_scan_service.h"
#include "veyra/runtime/extension_engine.h"
#include "veyra/runtime/shell_ui_bridge.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace veyra {

struct DownloadArtifactRecord {
  std::string artifact_id;
  std::string tab_id;
  std::string source_url;
  std::string suggested_filename;
  std::string quarantine_path;
  std::string quarantine_uri;
  std::string released_path;
  std::string report_path;
  std::string event_log_path;
  std::uint64_t received_bytes = 0;
  bool failed = false;
};

struct WebKitGtkRuntimeState {
  WebKitGtkBrowserEngine* owner = nullptr;
  GtkWidget* window = nullptr;
  GtkWidget* paned = nullptr;
  GtkWidget* dashboard_view_widget = nullptr;
  WebKitWebView* dashboard_view = nullptr;
  GtkWidget* notebook = nullptr;
  GtkWidget* address_entry = nullptr;
  GtkWidget* back_button = nullptr;
  GtkWidget* forward_button = nullptr;
  WebKitWebsiteDataManager* website_data_manager = nullptr;
  WebKitWebContext* web_context = nullptr;
  std::unordered_map<std::string, WebKitWebView*> tabs;
  std::unordered_map<WebKitWebView*, std::string> tab_ids;
  std::unordered_map<GtkWidget*, std::string> page_ids;
  std::unordered_map<WebKitDownload*, DownloadArtifactRecord> downloads;
  std::unordered_map<std::string, bool> permission_session_decisions;
  std::string active_tab_id;
  std::uint64_t artifact_counter = 0;
};

namespace {

void SetError(std::string* error, const std::string& message) {
  if (error != nullptr) {
    *error = message;
  }
}

bool IsValidUri(const std::string& url) {
  return !url.empty() && g_uri_is_valid(url.c_str(), G_URI_FLAGS_NONE, nullptr);
}

bool IsAsciiDigitString(const std::string& value) {
  for (const char ch : value) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return !value.empty();
}

bool IsLocalhostHost(const std::string& host) {
  return host == "localhost" || host == "::1" || host == "127.0.0.1";
}

bool IsPrivateIpv4(const std::string& host) {
  if (!g_hostname_is_ip_address(host.c_str())) {
    return false;
  }

  if (host.rfind("10.", 0) == 0 || host.rfind("127.", 0) == 0 || host.rfind("192.168.", 0) == 0) {
    return true;
  }

  if (host.rfind("172.", 0) == 0) {
    const std::size_t second_dot = host.find('.', 4);
    const std::string second_octet = host.substr(4, second_dot == std::string::npos ? std::string::npos
                                                                                     : second_dot - 4);
    if (IsAsciiDigitString(second_octet)) {
      const int value = std::stoi(second_octet);
      return value >= 16 && value <= 31;
    }
  }

  return false;
}

bool IsPrivateIpv6(const std::string& host) {
  return host == "::1" || host.rfind("fc", 0) == 0 || host.rfind("fd", 0) == 0 ||
         host.rfind("fe80", 0) == 0;
}

std::string ToLowerAscii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string SanitizeFileName(std::string value) {
  if (value.empty()) {
    return "download.bin";
  }
  for (char& ch : value) {
    if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '-' || ch == '_')) {
      ch = '_';
    }
  }
  return value;
}

std::string JsonEscape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

std::string BuildPromptCacheKey(const std::string& origin, PermissionKind permission) {
  return origin + "::" + ToString(permission);
}

bool ParseStoredPermissionDecision(const std::string& line,
                                   std::string* origin,
                                   PermissionKind* permission,
                                   bool* allowed) {
  const std::size_t first_tab = line.find('\t');
  const std::size_t second_tab =
      first_tab == std::string::npos ? std::string::npos : line.find('\t', first_tab + 1);
  if (first_tab == std::string::npos || second_tab == std::string::npos) {
    return false;
  }

  const std::string parsed_origin = line.substr(0, first_tab);
  const std::string permission_name = line.substr(first_tab + 1, second_tab - first_tab - 1);
  const std::string allow_value = line.substr(second_tab + 1);

  PermissionKind parsed_permission;
  if (permission_name == "notifications") {
    parsed_permission = PermissionKind::kNotifications;
  } else if (permission_name == "fullscreen") {
    parsed_permission = PermissionKind::kFullscreen;
  } else if (permission_name == "clipboard") {
    parsed_permission = PermissionKind::kClipboard;
  } else if (permission_name == "geolocation") {
    parsed_permission = PermissionKind::kGeolocation;
  } else if (permission_name == "camera") {
    parsed_permission = PermissionKind::kCamera;
  } else if (permission_name == "microphone") {
    parsed_permission = PermissionKind::kMicrophone;
  } else if (permission_name == "usb") {
    parsed_permission = PermissionKind::kUsb;
  } else if (permission_name == "filesystem") {
    parsed_permission = PermissionKind::kFilesystem;
  } else {
    return false;
  }

  if (origin != nullptr) {
    *origin = parsed_origin;
  }
  if (permission != nullptr) {
    *permission = parsed_permission;
  }
  if (allowed != nullptr) {
    *allowed = allow_value == "1" || allow_value == "allow" || allow_value == "true";
  }
  return true;
}

void AddCssProviderToWidget(GtkWidget* widget, GtkCssProvider* provider) {
  if (widget == nullptr || provider == nullptr) {
    return;
  }
  gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
                                 GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

std::string ExtractOriginForPrompt(const std::string& url) {
  if (!IsValidUri(url)) {
    return url.empty() ? "unknown-origin" : url;
  }

  GUri* parsed_uri = g_uri_parse(url.c_str(), G_URI_FLAGS_NONE, nullptr);
  if (parsed_uri == nullptr) {
    return url;
  }

  const gchar* scheme_value = g_uri_get_scheme(parsed_uri);
  const gchar* host_value = g_uri_get_host(parsed_uri);
  const gint port_value = g_uri_get_port(parsed_uri);

  std::string origin;
  if (scheme_value != nullptr) {
    origin += ToLowerAscii(std::string(scheme_value));
    origin += "://";
  }
  if (host_value != nullptr) {
    origin += ToLowerAscii(std::string(host_value));
  }
  if (port_value > 0) {
    origin += ":" + std::to_string(port_value);
  }

  g_uri_unref(parsed_uri);
  return origin.empty() ? url : origin;
}

WebKitCookieAcceptPolicy ResolveCookieAcceptPolicy(const EngineSecurityPolicy& policy) {
  if (policy.cookie_policy == "standard") {
    return WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
  }
  if (policy.cookie_policy == "strict") {
    return WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;
  }
  return policy.block_third_party_cookies ? WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY
                                          : WEBKIT_COOKIE_POLICY_ACCEPT_NEVER;
}

void OnWindowDestroy(GtkWidget* /*widget*/, gpointer /*user_data*/) {
  gtk_main_quit();
}

WebKitWebView* ActiveView(WebKitGtkRuntimeState* state) {
  if (state == nullptr || state->active_tab_id.empty()) {
    return nullptr;
  }
  const auto it = state->tabs.find(state->active_tab_id);
  return it == state->tabs.end() ? nullptr : it->second;
}

// Reflect the active tab's URL and back/forward availability in the toolbar.
void SyncToolbar(WebKitGtkRuntimeState* state) {
  if (state == nullptr) {
    return;
  }
  WebKitWebView* view = ActiveView(state);
  if (state->address_entry != nullptr) {
    const gchar* uri = view != nullptr ? webkit_web_view_get_uri(view) : nullptr;
    gtk_entry_set_text(GTK_ENTRY(state->address_entry), uri == nullptr ? "" : uri);
  }
  if (state->back_button != nullptr) {
    gtk_widget_set_sensitive(state->back_button,
                             view != nullptr && webkit_web_view_can_go_back(view));
  }
  if (state->forward_button != nullptr) {
    gtk_widget_set_sensitive(state->forward_button,
                             view != nullptr && webkit_web_view_can_go_forward(view));
  }
}

void OnNotebookSwitchPage(GtkNotebook* /*notebook*/, GtkWidget* page, guint /*page_num*/, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || page == nullptr) {
    return;
  }

  const auto found = state->page_ids.find(page);
  if (found != state->page_ids.end()) {
    state->active_tab_id = found->second;
    SyncToolbar(state);
  }
}

void OnNavBack(GtkButton* /*button*/, gpointer user_data) {
  WebKitWebView* view = ActiveView(static_cast<WebKitGtkRuntimeState*>(user_data));
  if (view != nullptr && webkit_web_view_can_go_back(view)) {
    webkit_web_view_go_back(view);
  }
}

void OnNavForward(GtkButton* /*button*/, gpointer user_data) {
  WebKitWebView* view = ActiveView(static_cast<WebKitGtkRuntimeState*>(user_data));
  if (view != nullptr && webkit_web_view_can_go_forward(view)) {
    webkit_web_view_go_forward(view);
  }
}

void OnNavReload(GtkButton* /*button*/, gpointer user_data) {
  WebKitWebView* view = ActiveView(static_cast<WebKitGtkRuntimeState*>(user_data));
  if (view != nullptr) {
    webkit_web_view_reload(view);
  }
}

void OnAddressActivate(GtkEntry* entry, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  WebKitWebView* view = ActiveView(state);
  if (view == nullptr) {
    return;
  }
  const gchar* raw = gtk_entry_get_text(entry);
  if (raw == nullptr) {
    return;
  }
  std::string target(raw);
  // Trim whitespace.
  while (!target.empty() && std::isspace(static_cast<unsigned char>(target.front()))) {
    target.erase(target.begin());
  }
  while (!target.empty() && std::isspace(static_cast<unsigned char>(target.back()))) {
    target.pop_back();
  }
  if (target.empty()) {
    return;
  }
  // Normalize: add a scheme if the operator typed a bare host/domain.
  const bool has_scheme = target.find("://") != std::string::npos ||
                          target.rfind("about:", 0) == 0 ||
                          target.rfind("file:", 0) == 0;
  if (!has_scheme) {
    // A token with a dot and no spaces looks like a host; otherwise treat as a
    // DuckDuckGo search query (privacy-respecting default).
    if (target.find(' ') == std::string::npos && target.find('.') != std::string::npos) {
      target = "https://" + target;
    } else {
      gchar* escaped = g_uri_escape_string(target.c_str(), nullptr, FALSE);
      target = "https://duckduckgo.com/?q=" + std::string(escaped == nullptr ? "" : escaped);
      if (escaped != nullptr) g_free(escaped);
    }
  }
  webkit_web_view_load_uri(view, target.c_str());
}

void OnNewTabClicked(GtkButton* /*button*/, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state != nullptr && state->owner != nullptr) {
    // Route through the dashboard action path so main.cc creates the tab in both
    // the browser-window model and the engine, keeping them in sync.
    state->owner->DispatchDashboardAction(
        "{\"action\":\"open_tab\",\"url\":\"about:blank\"}");
  }
}

// Builds the browser chrome: a vertical box of [navigation toolbar | notebook].
GtkWidget* BuildBrowserChrome(WebKitGtkRuntimeState* state) {
  GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_margin_start(toolbar, 6);
  gtk_widget_set_margin_end(toolbar, 6);
  gtk_widget_set_margin_top(toolbar, 5);
  gtk_widget_set_margin_bottom(toolbar, 5);

  state->back_button = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(state->back_button, "Back");
  g_signal_connect(state->back_button, "clicked", G_CALLBACK(OnNavBack), state);

  state->forward_button = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(state->forward_button, "Forward");
  g_signal_connect(state->forward_button, "clicked", G_CALLBACK(OnNavForward), state);

  GtkWidget* reload_button = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(reload_button, "Reload");
  g_signal_connect(reload_button, "clicked", G_CALLBACK(OnNavReload), state);

  state->address_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(state->address_entry),
                                 "Search DuckDuckGo or enter address");
  gtk_entry_set_icon_from_icon_name(GTK_ENTRY(state->address_entry),
                                    GTK_ENTRY_ICON_PRIMARY, "system-search-symbolic");
  gtk_widget_set_hexpand(state->address_entry, TRUE);
  g_signal_connect(state->address_entry, "activate", G_CALLBACK(OnAddressActivate), state);

  GtkWidget* new_tab_button = gtk_button_new_from_icon_name("tab-new-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(new_tab_button, "New tab");
  g_signal_connect(new_tab_button, "clicked", G_CALLBACK(OnNewTabClicked), state);

  gtk_box_pack_start(GTK_BOX(toolbar), state->back_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar), state->forward_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar), reload_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar), state->address_entry, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(toolbar), new_tab_button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), state->notebook, TRUE, TRUE, 0);
  return vbox;
}

// Snapshots a WebKitWebView's rendered content to PNG via WebKit's own snapshot
// API (GDK window grabs miss GL-composited WebKit surfaces, hence this path).
struct SnapshotContext {
  std::string path;
};

void OnSnapshotReady(GObject* source, GAsyncResult* result, gpointer user_data) {
  auto* ctx = static_cast<SnapshotContext*>(user_data);
  GError* gerror = nullptr;
  cairo_surface_t* surface =
      webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(source), result, &gerror);
  bool ok = false;
  if (surface != nullptr) {
    ok = cairo_surface_write_to_png(surface, ctx->path.c_str()) == CAIRO_STATUS_SUCCESS;
    cairo_surface_destroy(surface);
  }
  if (gerror != nullptr) {
    g_error_free(gerror);
  }
  std::cout << "[control] snapshot " << (ok ? "ok " : "fail ") << ctx->path << "\n" << std::flush;
  delete ctx;
}

void SnapshotView(WebKitWebView* view, const std::string& path) {
  if (view == nullptr) {
    std::cout << "[control] snapshot fail (no view) " << path << "\n" << std::flush;
    return;
  }
  auto* ctx = new SnapshotContext{path};
  webkit_web_view_get_snapshot(view, WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT,
                               WEBKIT_SNAPSHOT_OPTIONS_NONE, nullptr,
                               OnSnapshotReady, ctx);
}

// Reports a control result to stdout (prefixed, with optional @id) and a
// companion .out file (or .out.<id> when an id is present) for scripted reads.
void WriteControlResult(const std::string& out_path, const std::string& id,
                        const std::string& text) {
  std::cout << "[control] result " << (id.empty() ? "" : ("@" + id + " ")) << text
            << "\n" << std::flush;
  if (!out_path.empty()) {
    std::ofstream f(out_path, std::ios::out | std::ios::trunc);
    if (f.is_open()) {
      f << text;
    }
  }
}

struct ControlEvalContext {
  std::string out_path;
  std::string id;
};

void OnControlEvalReady(GObject* source, GAsyncResult* result, gpointer user_data) {
  auto* ctx = static_cast<ControlEvalContext*>(user_data);
  GError* gerror = nullptr;
  JSCValue* value =
      webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &gerror);
  std::string out;
  if (value != nullptr) {
    gchar* str = jsc_value_to_string(value);
    if (str != nullptr) {
      out.assign(str);
      g_free(str);
    }
    g_object_unref(value);
  } else if (gerror != nullptr) {
    out = std::string("{\"ok\":false,\"error\":\"") +
          (gerror->message ? gerror->message : "eval failed") + "\"}";
  }
  if (gerror != nullptr) {
    g_error_free(gerror);
  }
  WriteControlResult(ctx->out_path, ctx->id, out);
  delete ctx;
}

// ── wait-* polling infrastructure ────────────────────────────────────
struct ControlWaitContext {
  WebKitWebView* view = nullptr;
  std::string predicate_js;   // JS returning "true"/"false"; empty => use is_loading
  bool wait_for_load = false; // true => satisfied when the view stops loading
  gint64 deadline_us = 0;
  std::string out_path;
  std::string id;
  std::string label;          // human description for the result
  bool in_flight = false;
  bool satisfied = false;
};

void OnWaitEvalDone(GObject* source, GAsyncResult* result, gpointer user_data) {
  auto* ctx = static_cast<ControlWaitContext*>(user_data);
  GError* gerror = nullptr;
  JSCValue* value =
      webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &gerror);
  if (value != nullptr) {
    if (jsc_value_is_boolean(value)) {
      ctx->satisfied = jsc_value_to_boolean(value);
    } else {
      gchar* s = jsc_value_to_string(value);
      if (s != nullptr) { ctx->satisfied = (std::string(s) == "true"); g_free(s); }
    }
    g_object_unref(value);
  }
  if (gerror != nullptr) { g_error_free(gerror); }
  ctx->in_flight = false;
}

gboolean OnWaitTick(gpointer user_data) {
  auto* ctx = static_cast<ControlWaitContext*>(user_data);

  if (ctx->wait_for_load && !ctx->satisfied) {
    ctx->satisfied = (webkit_web_view_is_loading(ctx->view) == FALSE);
  }

  if (ctx->satisfied) {
    WriteControlResult(ctx->out_path, ctx->id,
                       "{\"ok\":true,\"satisfied\":true,\"wait\":\"" + ctx->label + "\"}");
    delete ctx;
    return G_SOURCE_REMOVE;
  }
  if (g_get_monotonic_time() > ctx->deadline_us) {
    WriteControlResult(ctx->out_path, ctx->id,
                       "{\"ok\":false,\"satisfied\":false,\"error\":\"timeout\",\"wait\":\"" +
                           ctx->label + "\"}");
    delete ctx;
    return G_SOURCE_REMOVE;
  }
  if (!ctx->wait_for_load && !ctx->in_flight && !ctx->predicate_js.empty()) {
    ctx->in_flight = true;
    webkit_web_view_evaluate_javascript(ctx->view, ctx->predicate_js.c_str(), -1, nullptr,
                                        nullptr, nullptr, OnWaitEvalDone, ctx);
  }
  return G_SOURCE_CONTINUE;
}

void StartControlWait(WebKitWebView* view, const std::string& predicate_js, bool wait_for_load,
                      int timeout_ms, const std::string& out_path, const std::string& id,
                      const std::string& label) {
  if (view == nullptr) {
    WriteControlResult(out_path, id, "{\"ok\":false,\"error\":\"no active tab\"}");
    return;
  }
  auto* ctx = new ControlWaitContext();
  ctx->view = view;
  ctx->predicate_js = predicate_js;
  ctx->wait_for_load = wait_for_load;
  ctx->deadline_us = g_get_monotonic_time() + static_cast<gint64>(timeout_ms) * 1000;
  ctx->out_path = out_path;
  ctx->id = id;
  ctx->label = label;
  g_timeout_add(120, OnWaitTick, ctx);
}

gboolean OnControlReadable(GIOChannel* channel, GIOCondition condition, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || (condition & G_IO_ERR)) {
    return TRUE;
  }
  gchar* line = nullptr;
  gsize length = 0;
  GError* gerror = nullptr;
  while (g_io_channel_read_line(channel, &line, &length, nullptr, &gerror) == G_IO_STATUS_NORMAL &&
         line != nullptr) {
    std::string command(line);
    g_free(line);
    line = nullptr;
    // Strip trailing newline/whitespace.
    while (!command.empty() &&
           (command.back() == '\n' || command.back() == '\r' || command.back() == ' ')) {
      command.pop_back();
    }
    if (!command.empty() && state->owner != nullptr) {
      state->owner->HandleControlLine(command);
    }
  }
  if (gerror != nullptr) {
    g_error_free(gerror);
  }
  return TRUE;  // keep the watch alive
}

void OnLoadChanged(WebKitWebView* view, WebKitLoadEvent load_event, gpointer user_data) {
  if (load_event != WEBKIT_LOAD_COMMITTED && load_event != WEBKIT_LOAD_FINISHED) {
    return;
  }

  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return;
  }

  const auto found = state->tab_ids.find(view);
  if (found == state->tab_ids.end()) {
    return;
  }

  const gchar* uri = webkit_web_view_get_uri(view);
  const gchar* title = webkit_web_view_get_title(view);

  state->owner->RecordNavigation(found->second,
                                 uri == nullptr ? std::string() : std::string(uri),
                                 title == nullptr ? std::string("Untitled") : std::string(title));

  // Keep the address bar / nav buttons in sync when the active tab navigates.
  if (found->second == state->active_tab_id) {
    SyncToolbar(state);
  }
}

gboolean OnPermissionRequest(WebKitWebView* view,
                             WebKitPermissionRequest* permission_request,
                             gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return FALSE;
  }

  const auto found = state->tab_ids.find(view);
  const std::string tab_id = found == state->tab_ids.end() ? std::string("unknown") : found->second;

  std::string message;
  const bool allow = state->owner->HandlePermissionRequest(tab_id, permission_request, &message);
  if (allow) {
    webkit_permission_request_allow(permission_request);
  } else {
    webkit_permission_request_deny(permission_request);
  }

  if (!message.empty()) {
    state->owner->EmitPolicyEvent(tab_id, "permission", message);
  }

  return TRUE;
}

gboolean OnEnterFullscreen(WebKitWebView* view, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return FALSE;
  }

  const auto found = state->tab_ids.find(view);
  const std::string tab_id = found == state->tab_ids.end() ? std::string("unknown") : found->second;
  std::string message;
  if (!state->owner->ResolvePermissionDecision(
          tab_id, PermissionKind::kFullscreen, "fullscreen permission",
          "The current page requested fullscreen presentation.", &message)) {
    state->owner->EmitPolicyEvent(tab_id, "permission", "Blocked fullscreen request. " + message);
    return TRUE;
  }

  return FALSE;
}

gboolean OnRunFileChooser(WebKitWebView* view,
                          WebKitFileChooserRequest* request,
                          gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return FALSE;
  }

  const auto found = state->tab_ids.find(view);
  const std::string tab_id = found == state->tab_ids.end() ? std::string("unknown") : found->second;
  std::string message;
  if (state->owner->ResolvePermissionDecision(
          tab_id, PermissionKind::kFilesystem, "filesystem permission",
          "The current page requested local file selection access.", &message)) {
    return FALSE;
  }

  webkit_file_chooser_request_cancel(request);
  state->owner->EmitPolicyEvent(tab_id, "permission", "Blocked file chooser request. " + message);
  return TRUE;
}

gboolean OnShowNotification(WebKitWebView* view,
                            WebKitNotification* notification,
                            gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return FALSE;
  }

  const auto found = state->tab_ids.find(view);
  const std::string tab_id = found == state->tab_ids.end() ? std::string("unknown") : found->second;
  std::string message;
  if (state->owner->ResolvePermissionDecision(
          tab_id, PermissionKind::kNotifications, "notification permission",
          "The current page requested the ability to show desktop notifications.", &message)) {
    return FALSE;
  }

  webkit_notification_close(notification);
  state->owner->EmitPolicyEvent(tab_id, "permission", "Blocked site notification. " + message);
  return TRUE;
}

gboolean OnLoadFailedWithTlsErrors(WebKitWebView* view,
                                   const gchar* failing_uri,
                                   GTlsCertificate* /*certificate*/,
                                   GTlsCertificateFlags /*errors*/,
                                   gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return FALSE;
  }

  const auto found = state->tab_ids.find(view);
  const std::string tab_id = found == state->tab_ids.end() ? std::string("unknown") : found->second;
  const std::string uri = failing_uri == nullptr ? std::string("<unknown>") : std::string(failing_uri);
  state->owner->EmitPolicyEvent(tab_id, "network", "TLS validation failed for navigation: " + uri);
  return FALSE;
}

gboolean OnDecidePolicy(WebKitWebView* view,
                        WebKitPolicyDecision* decision,
                        WebKitPolicyDecisionType type,
                        gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr) {
    return FALSE;
  }

  const auto found = state->tab_ids.find(view);
  const std::string tab_id = found == state->tab_ids.end() ? std::string("unknown") : found->second;

  if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION ||
      !WEBKIT_IS_NAVIGATION_POLICY_DECISION(decision)) {
    webkit_policy_decision_use(decision);
    return TRUE;
  }

  auto* navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
  WebKitNavigationAction* action =
      webkit_navigation_policy_decision_get_navigation_action(navigation_decision);
  if (action == nullptr) {
    webkit_policy_decision_use(decision);
    return TRUE;
  }

  WebKitURIRequest* request = webkit_navigation_action_get_request(action);
  const gchar* uri = request == nullptr ? nullptr : webkit_uri_request_get_uri(request);
  const std::string resolved_uri = uri == nullptr ? std::string() : std::string(uri);

  std::string message;
  if (!state->owner->ShouldAllowNavigation(tab_id, resolved_uri, &message)) {
    webkit_policy_decision_ignore(decision);
    state->owner->EmitPolicyEvent(tab_id, "navigation", message);
    return TRUE;
  }

  // Third-party frame enforcement: block cross-origin iframe loads when the
  // active extension policy sets allow_third_party_frames = false.
  const gchar* frame_name_ptr =
      webkit_navigation_policy_decision_get_frame_name(navigation_decision);
  if (frame_name_ptr != nullptr) {
    std::string frame_message;
    if (state->owner->IsThirdPartyFrameBlocked(tab_id, resolved_uri, &frame_message)) {
      webkit_policy_decision_ignore(decision);
      state->owner->EmitPolicyEvent(tab_id, "extension", frame_message);
      return TRUE;
    }
  }

  webkit_policy_decision_use(decision);
  return TRUE;
}

void OnDashboardMessage(WebKitUserContentManager* /*manager*/,
                        WebKitJavascriptResult* js_result,
                        gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || js_result == nullptr) {
    return;
  }

  JSCValue* value = webkit_javascript_result_get_js_value(js_result);
  if (value == nullptr || !jsc_value_is_string(value)) {
    return;
  }

  gchar* raw = jsc_value_to_string(value);
  if (raw == nullptr) {
    return;
  }

  const std::string json(raw);
  g_free(raw);

  state->owner->DispatchDashboardAction(json);
}

void OnDownloadStarted(WebKitWebContext* /*context*/, WebKitDownload* download, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || download == nullptr) {
    return;
  }
  state->owner->HandleDownloadStarted(download);
}

gboolean OnDownloadDecideDestination(WebKitDownload* download,
                                     gchar* suggested_filename,
                                     gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || download == nullptr) {
    return FALSE;
  }
  return state->owner->DecideDownloadDestination(
             download,
             suggested_filename == nullptr ? std::string("download.bin")
                                           : std::string(suggested_filename))
             ? TRUE
             : FALSE;
}

void OnDownloadCreatedDestination(WebKitDownload* download,
                                  gchar* destination,
                                  gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || download == nullptr) {
    return;
  }
  const auto found = state->downloads.find(download);
  if (found == state->downloads.end()) {
    return;
  }
  state->owner->EmitArtifactEvent("destination-created", found->second.artifact_id,
                                  found->second.tab_id,
                                  destination == nullptr ? std::string()
                                                         : std::string(destination));
}

void OnDownloadReceivedData(WebKitDownload* download, guint64 data_length, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || download == nullptr) {
    return;
  }
  state->owner->UpdateDownloadProgress(download, static_cast<std::uint64_t>(data_length));
}

void OnDownloadFailed(WebKitDownload* download, GError* download_error, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || download == nullptr) {
    return;
  }
  state->owner->MarkDownloadFailed(download,
                                   download_error == nullptr ? std::string("unknown download error")
                                                             : std::string(download_error->message));
}

void OnDownloadFinished(WebKitDownload* download, gpointer user_data) {
  auto* state = static_cast<WebKitGtkRuntimeState*>(user_data);
  if (state == nullptr || state->owner == nullptr || download == nullptr) {
    return;
  }
  state->owner->CompleteDownload(download);
}

}  // namespace

WebKitGtkBrowserEngine::WebKitGtkBrowserEngine() : state_(new WebKitGtkRuntimeState()) {
  state_->owner = this;
}

WebKitGtkBrowserEngine::~WebKitGtkBrowserEngine() {
  if (state_ != nullptr) {
    if (state_->web_context != nullptr) {
      g_object_unref(state_->web_context);
      state_->web_context = nullptr;
    }
    if (state_->website_data_manager != nullptr) {
      g_object_unref(state_->website_data_manager);
      state_->website_data_manager = nullptr;
    }
  }
  delete state_;
  state_ = nullptr;
}

bool WebKitGtkBrowserEngine::CreateWindow(const BrowserEngineConfig& config, std::string* error) {
  config_ = config;

  int argc = 0;
  char** argv = nullptr;
  if (!gtk_init_check(&argc, &argv)) {
    SetError(error, "Failed to initialize GTK runtime.");
    return false;
  }

  state_->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (state_->window == nullptr) {
    SetError(error, "Failed to create GTK window.");
    return false;
  }

  if (!ApplyContextSecurityPolicy(error)) {
    return false;
  }

  if (!EnsureDownloadDirectories(error)) {
    return false;
  }

  if (!LoadPromptDecisionStore(error)) {
    return false;
  }

  gtk_window_set_title(GTK_WINDOW(state_->window), config_.window_title.c_str());
  gtk_window_set_default_size(GTK_WINDOW(state_->window), config_.width, config_.height);

  state_->notebook = gtk_notebook_new();

  // Phase 9: Dashboard sidebar via GtkPaned.
  // Left pane = React dashboard WebView (320px); right pane = browser notebook.
  if (!config_.shell_ui_dist_path.empty()) {
    // Resolve to an absolute path: a file:// URI built from a relative path
    // (e.g. "apps/shell-ui/dist") would treat the first path segment as the
    // URI host, yielding file://apps/... and a 404. An absolute path starts
    // with '/', giving the correct file:///abs/path form.
    std::error_code abs_ec;
    std::filesystem::path dist_abs =
        std::filesystem::absolute(config_.shell_ui_dist_path, abs_ec);
    if (abs_ec) {
      dist_abs = std::filesystem::path(config_.shell_ui_dist_path);
    }
    const std::string index_path = (dist_abs / "index.html").string();
    gchar* uri_c = g_filename_to_uri(index_path.c_str(), nullptr, nullptr);
    const std::string index_uri =
        uri_c != nullptr ? std::string(uri_c) : ("file://" + index_path);
    if (uri_c != nullptr) {
      g_free(uri_c);
    }

    // Dashboard uses an ephemeral context so it never stores data on disk.
    WebKitWebsiteDataManager* dash_data = webkit_website_data_manager_new_ephemeral();
    WebKitWebContext* dash_context =
        webkit_web_context_new_with_website_data_manager(dash_data);
    g_object_unref(dash_data);  // context now holds the only reference
    webkit_web_context_set_sandbox_enabled(dash_context, TRUE);

    GtkWidget* dash_widget = webkit_web_view_new_with_context(dash_context);
    g_object_unref(dash_context);  // view holds the context reference from here

    state_->dashboard_view = WEBKIT_WEB_VIEW(dash_widget);
    state_->dashboard_view_widget = dash_widget;

    // Use the view's own UCM so scripts and message handlers are wired correctly.
    WebKitUserContentManager* view_ucm =
        webkit_web_view_get_user_content_manager(state_->dashboard_view);

    if (!config_.dashboard_state_script.empty()) {
      WebKitUserScript* state_script = webkit_user_script_new(
          config_.dashboard_state_script.c_str(),
          WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
          WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
          nullptr, nullptr);
      webkit_user_content_manager_add_script(view_ucm, state_script);
      webkit_user_script_unref(state_script);
    }

    webkit_user_content_manager_register_script_message_handler(view_ucm, "veyra");
    g_signal_connect(view_ucm, "script-message-received::veyra",
                     G_CALLBACK(OnDashboardMessage), state_);

    WebKitSettings* dash_settings = webkit_settings_new();
    webkit_settings_set_enable_javascript(dash_settings, TRUE);
    webkit_settings_set_enable_developer_extras(dash_settings, FALSE);
    webkit_web_view_set_settings(state_->dashboard_view, dash_settings);
    g_object_unref(dash_settings);

    webkit_web_view_load_uri(state_->dashboard_view, index_uri.c_str());

    GtkWidget* browser_box = BuildBrowserChrome(state_);
    state_->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(state_->paned), dash_widget, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(state_->paned), browser_box, TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(state_->paned), 300);
    gtk_container_add(GTK_CONTAINER(state_->window), state_->paned);
  } else {
    gtk_container_add(GTK_CONTAINER(state_->window), BuildBrowserChrome(state_));
  }

  g_signal_connect(state_->window, "destroy", G_CALLBACK(OnWindowDestroy), nullptr);
  g_signal_connect(state_->notebook, "switch-page", G_CALLBACK(OnNotebookSwitchPage), state_);
  g_signal_connect(state_->web_context, "download-started", G_CALLBACK(OnDownloadStarted), state_);

  gtk_widget_show_all(state_->window);

  // Optional automation/self-test control channel.
  if (!config_.control_fifo_path.empty()) {
    const std::string& fifo_path = config_.control_fifo_path;
    ::unlink(fifo_path.c_str());
    if (mkfifo(fifo_path.c_str(), 0600) == 0) {
      // Open O_RDWR so the watch never receives HUP when a writer closes.
      const int fd = ::open(fifo_path.c_str(), O_RDWR | O_NONBLOCK);
      if (fd >= 0) {
        GIOChannel* channel = g_io_channel_unix_new(fd);
        g_io_channel_set_close_on_unref(channel, TRUE);
        g_io_add_watch(channel, static_cast<GIOCondition>(G_IO_IN | G_IO_ERR),
                       OnControlReadable, state_);
        g_io_channel_unref(channel);
        std::cout << "[control] listening on FIFO: " << fifo_path << "\n";
      }
    } else {
      std::cerr << "[control] failed to create FIFO: " << fifo_path << "\n";
    }
  }

  return true;
}

void WebKitGtkBrowserEngine::HandleControlLine(const std::string& raw_line) {
  // Optional correlation id: "@<id> <command...>". Results echo "@<id>" and are
  // written to <fifo>.out.<id> (otherwise <fifo>.out).
  std::string line = raw_line;
  std::string id;
  if (!line.empty() && line[0] == '@') {
    const std::size_t sp = line.find(' ');
    id = line.substr(1, sp == std::string::npos ? std::string::npos : sp - 1);
    line = sp == std::string::npos ? std::string() : line.substr(sp + 1);
  }

  const std::size_t sep = line.find(' ');
  const std::string cmd = line.substr(0, sep);
  const std::string arg = sep == std::string::npos ? std::string() : line.substr(sep + 1);

  WebKitWebView* view = ActiveView(state_);

  const std::string out_path =
      config_.control_fifo_path.empty()
          ? std::string()
          : config_.control_fifo_path + ".out" + (id.empty() ? "" : ("." + id));

  auto report = [&](const std::string& text) { WriteControlResult(out_path, id, text); };

  // Runs JS in the active page and reports the result.
  auto run_js = [&](const std::string& js) {
    if (view == nullptr) {
      report("{\"ok\":false,\"error\":\"no active tab\"}");
      return;
    }
    auto* ctx = new ControlEvalContext{out_path, id};
    webkit_web_view_evaluate_javascript(view, js.c_str(), -1, nullptr, nullptr, nullptr,
                                        OnControlEvalReady, ctx);
  };
  auto call = [&](const std::string& method, const std::string& args) {
    run_js("window.__VEYRA_CONTROL__." + method + "(" + args + ")");
  };
  auto q = [](const std::string& s) { return "\"" + JsonEscape(s) + "\""; };
  auto split2 = [](const std::string& s, std::string* a, std::string* b) {
    const std::size_t bar = s.find('|');
    if (bar == std::string::npos) { *a = s; b->clear(); return; }
    *a = s.substr(0, bar);
    *b = s.substr(bar + 1);
  };
  auto parse_timeout = [](const std::string& s, std::string* sel, int* ms) {
    std::string a, b; const std::size_t bar = s.find('|');
    if (bar == std::string::npos) { *sel = s; *ms = 10000; return; }
    *sel = s.substr(0, bar);
    *ms = std::atoi(s.substr(bar + 1).c_str());
    if (*ms <= 0) *ms = 10000;
  };

  // ── navigation ────────────────────────────────────────────────
  if (cmd == "navigate" && !arg.empty()) {
    if (view != nullptr) {
      webkit_web_view_load_uri(view, arg.c_str());
      if (state_->address_entry != nullptr) gtk_entry_set_text(GTK_ENTRY(state_->address_entry), arg.c_str());
      report("{\"ok\":true,\"navigating\":" + q(arg) + "}");
    } else { report("{\"ok\":false,\"error\":\"no active tab\"}"); }
  } else if (cmd == "back") {
    if (view != nullptr && webkit_web_view_can_go_back(view)) webkit_web_view_go_back(view);
    report("{\"ok\":true}");
  } else if (cmd == "forward") {
    if (view != nullptr && webkit_web_view_can_go_forward(view)) webkit_web_view_go_forward(view);
    report("{\"ok\":true}");
  } else if (cmd == "reload") {
    if (view != nullptr) webkit_web_view_reload(view);
    report("{\"ok\":true}");
  } else if (cmd == "url") {
    report(GetActiveTabUrl());
  } else if (cmd == "title") {
    run_js("document.title");

  // ── waits ─────────────────────────────────────────────────────
  } else if (cmd == "wait-load") {
    const int ms = arg.empty() ? 15000 : std::atoi(arg.c_str());
    StartControlWait(view, "", true, ms > 0 ? ms : 15000, out_path, id, "load");
  } else if (cmd == "wait-for" && !arg.empty()) {
    std::string sel; int ms; parse_timeout(arg, &sel, &ms);
    StartControlWait(view, "window.__VEYRA_CONTROL__.predExists(" + q(sel) + ")", false, ms,
                     out_path, id, "for:" + sel);
  } else if (cmd == "wait-text" && !arg.empty()) {
    std::string sel, val; split2(arg, &sel, &val);
    StartControlWait(view, "window.__VEYRA_CONTROL__.predText(" + q(sel) + "," + q(val) + ")",
                     false, 10000, out_path, id, "text:" + sel);

  // ── DOM actions ───────────────────────────────────────────────
  } else if (cmd == "click" && !arg.empty()) {
    call("click", q(arg));
  } else if (cmd == "dblclick" && !arg.empty()) {
    call("dblclick", q(arg));
  } else if (cmd == "rightclick" && !arg.empty()) {
    call("rightclick", q(arg));
  } else if (cmd == "hover" && !arg.empty()) {
    call("hover", q(arg));
  } else if (cmd == "focus" && !arg.empty()) {
    call("focus", q(arg));
  } else if (cmd == "blur" && !arg.empty()) {
    call("blur", q(arg));
  } else if (cmd == "clear" && !arg.empty()) {
    call("clear", q(arg));
  } else if (cmd == "submit" && !arg.empty()) {
    call("submit", q(arg));
  } else if (cmd == "fill" && !arg.empty()) {
    std::string sel, val; split2(arg, &sel, &val); call("fill", q(sel) + "," + q(val));
  } else if (cmd == "type" && !arg.empty()) {
    std::string sel, val; split2(arg, &sel, &val); call("type", q(sel) + "," + q(val));
  } else if (cmd == "keypress" && !arg.empty()) {
    call("keypress", q(arg));
  } else if (cmd == "check" && !arg.empty()) {
    std::string sel, val; split2(arg, &sel, &val);
    call("setChecked", q(sel) + "," + ((val == "1" || val == "true") ? "true" : "false"));
  } else if (cmd == "select" && !arg.empty()) {
    std::string sel, val; split2(arg, &sel, &val); call("selectOption", q(sel) + "," + q(val));
  } else if (cmd == "scroll" && !arg.empty()) {
    call("scrollIntoView", q(arg));
  } else if (cmd == "scroll-by" && !arg.empty()) {
    std::string x, y; split2(arg, &x, &y);
    call("scrollBy", std::to_string(std::atoi(x.c_str())) + "," + std::to_string(std::atoi(y.c_str())));

  // ── queries / introspection ───────────────────────────────────
  } else if (cmd == "gettext" && !arg.empty()) {
    call("getText", q(arg));
  } else if (cmd == "getvalue" && !arg.empty()) {
    call("getValue", q(arg));
  } else if (cmd == "exists" && !arg.empty()) {
    call("exists", q(arg));
  } else if (cmd == "visible" && !arg.empty()) {
    call("visible", q(arg));
  } else if (cmd == "count" && !arg.empty()) {
    call("count", q(arg));
  } else if (cmd == "attrs" && !arg.empty()) {
    call("attrs", q(arg));
  } else if (cmd == "html" && !arg.empty()) {
    call("html", q(arg));
  } else if (cmd == "bounds" && !arg.empty()) {
    call("bounds", q(arg));
  } else if (cmd == "query" && !arg.empty()) {
    call("query", q(arg));
  } else if (cmd == "pageinfo") {
    call("pageInfo", "");
  } else if (cmd == "links") {
    call("links", "50");

  // ── computer-use surface ──────────────────────────────────────
  } else if (cmd == "clickable") {
    call("clickable", "");
  } else if (cmd == "textdump") {
    call("textDump", arg.empty() ? "6000" : std::to_string(std::atoi(arg.c_str())));
  } else if (cmd == "annotate") {
    call("annotate", "");
  } else if (cmd == "annotate-clear") {
    call("clearAnnotations", "");
  } else if (cmd == "click-index" && !arg.empty()) {
    call("clickIndex", std::to_string(std::atoi(arg.c_str())));

  // ── logs / storage ────────────────────────────────────────────
  } else if (cmd == "console") {
    call("getLogs", "");
  } else if (cmd == "errors") {
    call("getErrors", "");
  } else if (cmd == "clear-logs") {
    call("clearLogs", "");
  } else if (cmd == "cookies") {
    call("cookies", "");
  } else if (cmd == "storage-get" && !arg.empty()) {
    call("storageGet", q(arg));
  } else if (cmd == "storage-set" && !arg.empty()) {
    std::string k, v; split2(arg, &k, &v); call("storageSet", q(k) + "," + q(v));
  } else if (cmd == "storage-clear") {
    call("storageClear", "");

  // ── assertions ────────────────────────────────────────────────
  } else if (cmd == "assert-text" && !arg.empty()) {
    std::string sel, val; split2(arg, &sel, &val); call("assertText", q(sel) + "," + q(val));
  } else if (cmd == "assert-exists" && !arg.empty()) {
    call("assertExists", q(arg));
  } else if (cmd == "assert-url" && !arg.empty()) {
    const std::string u = GetActiveTabUrl();
    const bool pass = u.find(arg) != std::string::npos;
    report(std::string("{\"ok\":true,\"pass\":") + (pass ? "true" : "false") +
           ",\"actual\":" + q(u) + "}");

  // ── tabs ──────────────────────────────────────────────────────
  } else if (cmd == "tabs") {
    std::string json = "{\"ok\":true,\"value\":[";
    bool first = true;
    for (const auto& kv : state_->tabs) {
      if (!first) json += ",";
      const gchar* uri = webkit_web_view_get_uri(kv.second);
      json += "{\"id\":" + q(kv.first) + ",\"url\":" + q(uri ? uri : "") +
              ",\"active\":" + (kv.first == state_->active_tab_id ? "true" : "false") + "}";
      first = false;
    }
    report(json + "]}");
  } else if (cmd == "tab" && !arg.empty()) {
    std::string err;
    report(ActivateTab(arg, &err) ? "{\"ok\":true}" : ("{\"ok\":false,\"error\":" + q(err) + "}"));

  // ── settings / actions ────────────────────────────────────────
  } else if (cmd == "route" && !arg.empty()) {
    DispatchDashboardAction("{\"action\":\"switch_route\",\"route_profile_id\":\"" + arg + "\"}");
    report("{\"ok\":true,\"route\":" + q(arg) + "}");
  } else if (cmd == "action" && !arg.empty()) {
    DispatchDashboardAction(arg);
    report("{\"ok\":true}");
  } else if (cmd == "tab-new") {
    DispatchDashboardAction("{\"action\":\"open_tab\",\"url\":\"" +
                            (arg.empty() ? std::string("about:blank") : arg) + "\"}");
    report("{\"ok\":true}");

  // ── capture / lifecycle ───────────────────────────────────────
  } else if (cmd == "snapshot" && !arg.empty()) {
    SnapshotView(view, arg);
  } else if (cmd == "snapshot-dash" && !arg.empty()) {
    SnapshotView(state_->dashboard_view, arg);
  } else if (cmd == "eval" && !arg.empty()) {
    run_js(arg);
  } else if (cmd == "script" && !arg.empty()) {
    RunControlScript(arg);
  } else if (cmd == "quit") {
    gtk_main_quit();
  } else {
    report("{\"ok\":false,\"error\":\"unknown command: " + JsonEscape(cmd) + "\"}");
  }
}

namespace {
struct ControlScriptContext {
  WebKitGtkBrowserEngine* engine = nullptr;
  WebKitGtkRuntimeState* state = nullptr;
  std::deque<std::string> lines;
};

gboolean OnScriptTick(gpointer user_data) {
  auto* ctx = static_cast<ControlScriptContext*>(user_data);
  if (ctx->lines.empty()) {
    std::cout << "[control] script complete\n" << std::flush;
    delete ctx;
    return G_SOURCE_REMOVE;
  }
  // Pace through page loads: don't run the next line while the active tab is
  // still loading, so `navigate` is naturally followed by its load.
  WebKitWebView* view = ActiveView(ctx->state);
  if (view != nullptr && webkit_web_view_is_loading(view)) {
    return G_SOURCE_CONTINUE;
  }
  std::string next = ctx->lines.front();
  ctx->lines.pop_front();
  // Trim + skip blanks and comments.
  while (!next.empty() && (next.back() == '\r' || next.back() == ' ')) next.pop_back();
  std::size_t start = next.find_first_not_of(" \t");
  if (start != std::string::npos) next = next.substr(start);
  if (!next.empty() && next[0] != '#') {
    ctx->engine->HandleControlLine(next);
  }
  return G_SOURCE_CONTINUE;
}
}  // namespace

void WebKitGtkBrowserEngine::RunControlScript(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    WriteControlResult(config_.control_fifo_path.empty() ? std::string()
                                                         : config_.control_fifo_path + ".out",
                       "", "{\"ok\":false,\"error\":\"cannot open script: " + JsonEscape(path) + "\"}");
    return;
  }
  auto* ctx = new ControlScriptContext();
  ctx->engine = this;
  ctx->state = state_;
  std::string line;
  while (std::getline(input, line)) {
    ctx->lines.push_back(line);
  }
  std::cout << "[control] running script: " << path << " (" << ctx->lines.size() << " lines)\n"
            << std::flush;
  g_timeout_add(250, OnScriptTick, ctx);
}

bool WebKitGtkBrowserEngine::CreateTab(const std::string& tab_id,
                                       const std::string& initial_url,
                                       std::string* error) {
  if (state_->notebook == nullptr) {
    SetError(error, "CreateWindow must be called before CreateTab.");
    return false;
  }

  if (state_->tabs.find(tab_id) != state_->tabs.end()) {
    SetError(error, "Tab id already exists: " + tab_id);
    return false;
  }

  if (!IsValidUri(initial_url)) {
    SetError(error, "Initial URL is invalid for tab " + tab_id + ": " + initial_url);
    return false;
  }

  std::string navigation_message;
  if (!ShouldAllowNavigation(tab_id, initial_url, &navigation_message)) {
    SetError(error, navigation_message);
    return false;
  }

  GtkWidget* widget =
      state_->web_context != nullptr ? webkit_web_view_new_with_context(state_->web_context)
                                     : webkit_web_view_new();
  auto* view = WEBKIT_WEB_VIEW(widget);
  if (view == nullptr) {
    SetError(error, "Failed to create WebKit web view.");
    return false;
  }

  ApplyViewSecurityPolicy(view);
  InjectFingerprintScript(view);
  InjectExtensionPolicyScript(view);
  InjectExtensionContentScripts(view);
  g_signal_connect(view, "load-changed", G_CALLBACK(OnLoadChanged), state_);
  g_signal_connect(view, "permission-request", G_CALLBACK(OnPermissionRequest), state_);
  g_signal_connect(view, "decide-policy", G_CALLBACK(OnDecidePolicy), state_);
  g_signal_connect(view, "enter-fullscreen", G_CALLBACK(OnEnterFullscreen), state_);
  g_signal_connect(view, "run-file-chooser", G_CALLBACK(OnRunFileChooser), state_);
  g_signal_connect(view, "show-notification", G_CALLBACK(OnShowNotification), state_);
  g_signal_connect(view, "load-failed-with-tls-errors", G_CALLBACK(OnLoadFailedWithTlsErrors), state_);

  GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
  gtk_container_add(GTK_CONTAINER(scroller), GTK_WIDGET(view));

  GtkWidget* label = gtk_label_new(tab_id.c_str());
  const int page_index = gtk_notebook_append_page(GTK_NOTEBOOK(state_->notebook), scroller, label);
  if (page_index < 0) {
    SetError(error, "Failed to append tab to notebook.");
    return false;
  }

  gtk_notebook_set_current_page(GTK_NOTEBOOK(state_->notebook), page_index);
  gtk_widget_show_all(scroller);

  state_->tabs.emplace(tab_id, view);
  state_->tab_ids.emplace(view, tab_id);
  state_->page_ids.emplace(scroller, tab_id);
  state_->active_tab_id = tab_id;

  RecordNavigation(tab_id, initial_url, tab_id);
  webkit_web_view_load_uri(view, initial_url.c_str());
  return true;
}

bool WebKitGtkBrowserEngine::ActivateTab(const std::string& tab_id, std::string* error) {
  const auto found = state_->tabs.find(tab_id);
  if (found == state_->tabs.end()) {
    SetError(error, "Unknown tab id: " + tab_id);
    return false;
  }

  const int page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(state_->notebook));
  for (int i = 0; i < page_count; ++i) {
    GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(state_->notebook), i);
    if (page == nullptr) {
      continue;
    }

    const auto page_it = state_->page_ids.find(page);
    if (page_it != state_->page_ids.end() && page_it->second == tab_id) {
      gtk_notebook_set_current_page(GTK_NOTEBOOK(state_->notebook), i);
      state_->active_tab_id = tab_id;
      return true;
    }
  }

  SetError(error, "Unable to activate tab id: " + tab_id);
  return false;
}

bool WebKitGtkBrowserEngine::Navigate(const std::string& tab_id,
                                      const std::string& url,
                                      std::string* error) {
  const auto found = state_->tabs.find(tab_id);
  if (found == state_->tabs.end()) {
    SetError(error, "Unknown tab id: " + tab_id);
    return false;
  }

  if (url.empty()) {
    SetError(error, "Navigate requires a non-empty URL.");
    return false;
  }

  if (!IsValidUri(url)) {
    SetError(error, "Navigate received an invalid URL for tab " + tab_id + ": " + url);
    return false;
  }

  std::string navigation_message;
  if (!ShouldAllowNavigation(tab_id, url, &navigation_message)) {
    SetError(error, navigation_message);
    EmitPolicyEvent(tab_id, "navigation", navigation_message);
    return false;
  }

  webkit_web_view_load_uri(found->second, url.c_str());
  return true;
}

bool WebKitGtkBrowserEngine::GoBack(const std::string& tab_id, std::string* error) {
  const auto found = state_->tabs.find(tab_id);
  if (found == state_->tabs.end()) {
    SetError(error, "Unknown tab id: " + tab_id);
    return false;
  }

  if (!webkit_web_view_can_go_back(found->second)) {
    SetError(error, "No backward history available for tab: " + tab_id);
    return false;
  }

  webkit_web_view_go_back(found->second);
  return true;
}

bool WebKitGtkBrowserEngine::GoForward(const std::string& tab_id, std::string* error) {
  const auto found = state_->tabs.find(tab_id);
  if (found == state_->tabs.end()) {
    SetError(error, "Unknown tab id: " + tab_id);
    return false;
  }

  if (!webkit_web_view_can_go_forward(found->second)) {
    SetError(error, "No forward history available for tab: " + tab_id);
    return false;
  }

  webkit_web_view_go_forward(found->second);
  return true;
}

bool WebKitGtkBrowserEngine::ApplySecurityPolicy(
    const EngineSecurityPolicy& policy,
    const std::vector<PermissionEvaluation>& permission_report,
    std::string* error) {
  config_.security_policy = policy;
  config_.permission_report = permission_report;
  config_.fingerprint_script = policy.fingerprint_script;

  if (!EnsureDownloadDirectories(error)) {
    return false;
  }
  if (!ApplyNetworkProxySettings(policy, error)) {
    return false;
  }

  for (const auto& [tab_id, view] : state_->tabs) {
    if (view == nullptr) {
      continue;
    }
    ApplyViewSecurityPolicy(view);
    WebKitUserContentManager* ucm = webkit_web_view_get_user_content_manager(view);
    if (ucm != nullptr) {
      webkit_user_content_manager_remove_all_scripts(ucm);
      InjectFingerprintScript(view);
      InjectExtensionPolicyScript(view);
      InjectExtensionContentScripts(view);
    }
    EmitPolicyEvent(tab_id, "route", "Applied hot route/security policy update to active tab.");
    webkit_web_view_reload_bypass_cache(view);
  }
  return true;
}

std::string WebKitGtkBrowserEngine::GetActiveTabUrl() const {
  if (state_ == nullptr || state_->active_tab_id.empty()) {
    return {};
  }
  return GetCurrentUrl(state_->active_tab_id);
}

std::string WebKitGtkBrowserEngine::GetActiveTabTitle() const {
  if (state_ == nullptr || state_->active_tab_id.empty()) {
    return {};
  }
  const auto found = state_->tabs.find(state_->active_tab_id);
  if (found == state_->tabs.end()) {
    return {};
  }
  const gchar* title = webkit_web_view_get_title(found->second);
  return title == nullptr ? std::string() : std::string(title);
}

void WebKitGtkBrowserEngine::RequestActiveTabText(
    std::function<void(const std::string& text)> callback) {
  if (state_ == nullptr || state_->active_tab_id.empty()) {
    callback("");
    return;
  }
  const auto found = state_->tabs.find(state_->active_tab_id);
  if (found == state_->tabs.end()) {
    callback("");
    return;
  }

  // Heap-allocate the callback so it survives until the async JS result arrives.
  auto* ctx = new std::function<void(const std::string&)>(std::move(callback));

  const char* extract_js =
      "(function(){try{return (document.body&&document.body.innerText)||'';}"
      "catch(e){return '';}})();";

  webkit_web_view_evaluate_javascript(
      found->second, extract_js, -1, nullptr, nullptr, nullptr,
      [](GObject* source, GAsyncResult* result, gpointer user_data) {
        auto* cb = static_cast<std::function<void(const std::string&)>*>(user_data);
        std::string text;
        GError* error = nullptr;
        JSCValue* value = webkit_web_view_evaluate_javascript_finish(
            WEBKIT_WEB_VIEW(source), result, &error);
        if (value != nullptr) {
          if (jsc_value_is_string(value)) {
            gchar* raw = jsc_value_to_string(value);
            if (raw != nullptr) {
              text.assign(raw);
              g_free(raw);
            }
          }
          g_object_unref(value);
        }
        if (error != nullptr) {
          g_error_free(error);
        }
        (*cb)(text);
        delete cb;
      },
      ctx);
}

std::string WebKitGtkBrowserEngine::GetCurrentUrl(const std::string& tab_id) const {
  const auto known = current_urls_.find(tab_id);
  if (known != current_urls_.end()) {
    return known->second;
  }

  const auto found = state_->tabs.find(tab_id);
  if (found == state_->tabs.end()) {
    return {};
  }

  const gchar* uri = webkit_web_view_get_uri(found->second);
  if (uri == nullptr) {
    return {};
  }

  return std::string(uri);
}

void WebKitGtkBrowserEngine::Run() {
  gtk_main();
}

void WebKitGtkBrowserEngine::RecordNavigation(const std::string& tab_id,
                                              const std::string& url,
                                              const std::string& title) {
  std::string resolved_url = url;
  if (resolved_url.empty()) {
    resolved_url = GetCurrentUrl(tab_id);
  }

  if (resolved_url.empty()) {
    return;
  }

  current_urls_[tab_id] = resolved_url;

  if (config_.on_navigation_committed) {
    config_.on_navigation_committed(tab_id, resolved_url, title);
  }
}

bool WebKitGtkBrowserEngine::ApplyContextSecurityPolicy(std::string* error) {
  const EngineSecurityPolicy& policy = config_.security_policy;

  if (policy.use_ephemeral_context) {
    state_->website_data_manager = webkit_website_data_manager_new_ephemeral();
  } else {
    state_->website_data_manager = webkit_website_data_manager_new(
        "base-data-directory", policy.base_data_directory.c_str(),
        "base-cache-directory", policy.base_cache_directory.c_str(), nullptr);
  }

  if (state_->website_data_manager == nullptr) {
    SetError(error, "Failed to create WebKit website data manager.");
    return false;
  }

  WebKitCookieManager* cookie_manager =
      webkit_website_data_manager_get_cookie_manager(state_->website_data_manager);
  if (cookie_manager != nullptr) {
    webkit_cookie_manager_set_accept_policy(cookie_manager, ResolveCookieAcceptPolicy(policy));
    if (!policy.use_ephemeral_context && !policy.cookie_storage_path.empty()) {
      webkit_cookie_manager_set_persistent_storage(
          cookie_manager, policy.cookie_storage_path.c_str(), WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
    }
  }

  webkit_website_data_manager_set_itp_enabled(state_->website_data_manager, policy.enable_itp);
  webkit_website_data_manager_set_persistent_credential_storage_enabled(
      state_->website_data_manager, policy.persistent_credential_storage_enabled);
  webkit_website_data_manager_set_tls_errors_policy(
      state_->website_data_manager,
      policy.strict_tls_errors ? WEBKIT_TLS_ERRORS_POLICY_FAIL : WEBKIT_TLS_ERRORS_POLICY_IGNORE);
  if (!ApplyNetworkProxySettings(policy, error)) {
    return false;
  }

  state_->web_context =
      webkit_web_context_new_with_website_data_manager(state_->website_data_manager);
  if (state_->web_context == nullptr) {
    SetError(error, "Failed to create WebKit web context.");
    return false;
  }

  WebKitCacheModel cache_model = WEBKIT_CACHE_MODEL_WEB_BROWSER;
  if (!policy.enable_page_cache) {
    cache_model = WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER;
  } else if (policy.history_retention_policy == "reduced") {
    cache_model = WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER;
  }

  webkit_web_context_set_cache_model(state_->web_context, cache_model);
  webkit_web_context_set_sandbox_enabled(state_->web_context, policy.enable_sandbox);
  return true;
}

bool WebKitGtkBrowserEngine::ApplyNetworkProxySettings(const EngineSecurityPolicy& policy,
                                                       std::string* error) {
  if (state_ == nullptr || state_->website_data_manager == nullptr) {
    SetError(error, "WebKit website data manager is not ready for proxy configuration.");
    return false;
  }

  WebKitNetworkProxyMode proxy_mode = WEBKIT_NETWORK_PROXY_MODE_NO_PROXY;
  WebKitNetworkProxySettings* proxy_settings = nullptr;
  if (policy.proxy_mode == "system-default") {
    proxy_mode = WEBKIT_NETWORK_PROXY_MODE_DEFAULT;
  } else if (policy.proxy_mode == "custom-proxy" && !policy.route_proxy_uri.empty()) {
    proxy_mode = WEBKIT_NETWORK_PROXY_MODE_CUSTOM;
    proxy_settings = webkit_network_proxy_settings_new(policy.route_proxy_uri.c_str(), nullptr);
    if (proxy_settings != nullptr) {
      webkit_network_proxy_settings_add_proxy_for_scheme(
          proxy_settings, "http", policy.route_proxy_uri.c_str());
      webkit_network_proxy_settings_add_proxy_for_scheme(
          proxy_settings, "https", policy.route_proxy_uri.c_str());
      webkit_network_proxy_settings_add_proxy_for_scheme(
          proxy_settings, "ws", policy.route_proxy_uri.c_str());
      webkit_network_proxy_settings_add_proxy_for_scheme(
          proxy_settings, "wss", policy.route_proxy_uri.c_str());
    }
  }

  webkit_website_data_manager_set_network_proxy_settings(
      state_->website_data_manager, proxy_mode, proxy_settings);
  if (proxy_settings != nullptr) {
    webkit_network_proxy_settings_free(proxy_settings);
  }
  return true;
}

bool WebKitGtkBrowserEngine::EnsureDownloadDirectories(std::string* error) const {
  const EngineSecurityPolicy& policy = config_.security_policy;
  const std::filesystem::path directories[] = {
      std::filesystem::path(policy.download_root_directory),
      std::filesystem::path(policy.quarantine_root_directory),
      std::filesystem::path(policy.released_root_directory),
      std::filesystem::path(policy.artifact_report_directory),
  };

  std::error_code directory_error;
  for (const auto& directory : directories) {
    if (directory.empty()) {
      continue;
    }
    std::filesystem::create_directories(directory, directory_error);
    if (directory_error) {
      SetError(error, "Failed to prepare download pipeline directory: " + directory.string());
      return false;
    }
  }
  return true;
}

std::string WebKitGtkBrowserEngine::EmitArtifactEvent(const std::string& event_type,
                                                      const std::string& artifact_id,
                                                      const std::string& tab_id,
                                                      const std::string& details) const {
  const std::string& event_log_path = config_.security_policy.artifact_event_log_path;
  if (!event_log_path.empty()) {
    std::ofstream output(event_log_path, std::ios::out | std::ios::app);
    if (output.is_open()) {
      output << "{"
             << "\"event_type\":\"" << JsonEscape(event_type) << "\","
             << "\"artifact_id\":\"" << JsonEscape(artifact_id) << "\","
             << "\"tab_id\":\"" << JsonEscape(tab_id) << "\","
             << "\"details\":\"" << JsonEscape(details) << "\""
             << "}\n";
    }
  }

  const std::string resolved_tab_id = tab_id.empty() ? std::string("download") : tab_id;
  EmitPolicyEvent(resolved_tab_id, "download", event_type + ": " + details);
  return event_log_path;
}

void WebKitGtkBrowserEngine::HandleDownloadStarted(void* download_ptr) {
  auto* download = WEBKIT_DOWNLOAD(download_ptr);
  if (download == nullptr || state_ == nullptr) {
    return;
  }

  WebKitWebView* owner_view = webkit_download_get_web_view(download);
  std::string tab_id = "download";
  if (owner_view != nullptr) {
    const auto found = state_->tab_ids.find(owner_view);
    if (found != state_->tab_ids.end()) {
      tab_id = found->second;
    }
  }

  const WebKitURIRequest* request = webkit_download_get_request(download);
  const gchar* request_uri = request == nullptr ? nullptr : webkit_uri_request_get_uri(const_cast<WebKitURIRequest*>(request));

  const std::string artifact_id = "artifact-" + std::to_string(++state_->artifact_counter);
  state_->downloads[download] = DownloadArtifactRecord{
      artifact_id,
      tab_id,
      request_uri == nullptr ? std::string() : std::string(request_uri),
      std::string(),
      std::string(),
      std::string(),
      std::string(),
      std::string(),
      config_.security_policy.artifact_event_log_path,
      0,
      false,
  };

  g_signal_connect(download, "decide-destination", G_CALLBACK(OnDownloadDecideDestination), state_);
  g_signal_connect(download, "created-destination", G_CALLBACK(OnDownloadCreatedDestination), state_);
  g_signal_connect(download, "received-data", G_CALLBACK(OnDownloadReceivedData), state_);
  g_signal_connect(download, "failed", G_CALLBACK(OnDownloadFailed), state_);
  g_signal_connect(download, "finished", G_CALLBACK(OnDownloadFinished), state_);

  EmitArtifactEvent("intercepted", artifact_id, tab_id,
                    "Intercepted download request for quarantined handling.");
}

bool WebKitGtkBrowserEngine::DecideDownloadDestination(void* download_ptr,
                                                       const std::string& suggested_filename) {
  auto* download = WEBKIT_DOWNLOAD(download_ptr);
  if (download == nullptr || state_ == nullptr) {
    return false;
  }

  auto found = state_->downloads.find(download);
  if (found == state_->downloads.end()) {
    return false;
  }

  found->second.suggested_filename = suggested_filename;
  const std::string sanitized_filename = SanitizeFileName(suggested_filename);
  std::filesystem::path quarantine_path =
      std::filesystem::path(config_.security_policy.quarantine_root_directory) /
      (found->second.artifact_id + "-" + sanitized_filename);
  std::filesystem::path released_path =
      std::filesystem::path(config_.security_policy.released_root_directory) /
      sanitized_filename;
  std::filesystem::path report_path =
      std::filesystem::path(config_.security_policy.artifact_report_directory) /
      (found->second.artifact_id + ".json");

  std::error_code path_error;
  if (std::filesystem::exists(quarantine_path, path_error)) {
    quarantine_path = quarantine_path.parent_path() /
                      (found->second.artifact_id + "-1-" + sanitized_filename);
  }

  gchar* quarantine_uri =
      g_filename_to_uri(quarantine_path.string().c_str(), nullptr, nullptr);
  if (quarantine_uri == nullptr) {
    EmitArtifactEvent("destination-error", found->second.artifact_id, found->second.tab_id,
                      "Failed to convert quarantine path to file URI.");
    webkit_download_cancel(download);
    return true;
  }

  webkit_download_set_allow_overwrite(download, FALSE);
  webkit_download_set_destination(download, quarantine_uri);

  found->second.quarantine_path = quarantine_path.string();
  found->second.quarantine_uri = quarantine_uri;
  found->second.released_path = released_path.string();
  found->second.report_path = report_path.string();

  EmitArtifactEvent("quarantine-routed", found->second.artifact_id, found->second.tab_id,
                    "Download destination redirected to " + found->second.quarantine_path);
  g_free(quarantine_uri);
  return true;
}

void WebKitGtkBrowserEngine::UpdateDownloadProgress(void* download_ptr, std::uint64_t data_length) {
  auto* download = WEBKIT_DOWNLOAD(download_ptr);
  if (download == nullptr || state_ == nullptr) {
    return;
  }

  auto found = state_->downloads.find(download);
  if (found == state_->downloads.end()) {
    return;
  }

  found->second.received_bytes += data_length;
}

void WebKitGtkBrowserEngine::MarkDownloadFailed(void* download_ptr, const std::string& error_message) {
  auto* download = WEBKIT_DOWNLOAD(download_ptr);
  if (download == nullptr || state_ == nullptr) {
    return;
  }

  auto found = state_->downloads.find(download);
  if (found == state_->downloads.end()) {
    return;
  }

  found->second.failed = true;
  EmitArtifactEvent("failed", found->second.artifact_id, found->second.tab_id, error_message);
}

void WebKitGtkBrowserEngine::CompleteDownload(void* download_ptr) {
  auto* download = WEBKIT_DOWNLOAD(download_ptr);
  if (download == nullptr || state_ == nullptr) {
    return;
  }

  auto found = state_->downloads.find(download);
  if (found == state_->downloads.end()) {
    return;
  }

  DownloadArtifactRecord artifact = found->second;
  state_->downloads.erase(found);

  if (artifact.quarantine_path.empty()) {
    EmitArtifactEvent("finished-without-quarantine", artifact.artifact_id, artifact.tab_id,
                      "Download finished without a managed quarantine path.");
    return;
  }

  if (artifact.failed) {
    EmitArtifactEvent("finished-after-failure", artifact.artifact_id, artifact.tab_id,
                      "Download finished after a recorded failure state.");
    return;
  }

  ArtifactScanClient scanner(config_.artifact_scan_binary_path);
  ArtifactScanReport report;
  std::string scan_error;
  if (!scanner.Scan(ArtifactScanRequest{
                        artifact.artifact_id,
                        artifact.quarantine_path,
                        artifact.source_url,
                        artifact.suggested_filename,
                    },
                    &report, &scan_error)) {
    EmitArtifactEvent("scan-failed", artifact.artifact_id, artifact.tab_id, scan_error);
    return;
  }

  {
    std::ofstream output(artifact.report_path, std::ios::out | std::ios::trunc);
    if (output.is_open()) {
      output << "{\n"
             << "  \"artifact_id\": \"" << JsonEscape(report.artifact_id) << "\",\n"
             << "  \"quarantine_path\": \"" << JsonEscape(report.quarantine_path) << "\",\n"
             << "  \"source_url\": \"" << JsonEscape(report.source_url) << "\",\n"
             << "  \"suggested_filename\": \"" << JsonEscape(report.suggested_filename) << "\",\n"
             << "  \"detected_name\": \"" << JsonEscape(report.detected_name) << "\",\n"
             << "  \"size_bytes\": " << report.size_bytes << ",\n"
             << "  \"mime_guess\": \"" << JsonEscape(report.mime_guess) << "\",\n"
             << "  \"sha256\": \"" << JsonEscape(report.sha256) << "\",\n"
             << "  \"sha1\": \"" << JsonEscape(report.sha1) << "\",\n"
             << "  \"risk_score\": " << report.risk_score << ",\n"
             << "  \"risk_level\": \"" << JsonEscape(report.risk_level) << "\",\n"
             << "  \"heuristics\": [";
      for (std::size_t index = 0; index < report.heuristics.size(); ++index) {
        output << "\"" << JsonEscape(report.heuristics[index]) << "\"";
        if (index + 1 < report.heuristics.size()) {
          output << ", ";
        }
      }
      output << "],\n"
             << "  \"summary\": \"" << JsonEscape(report.summary) << "\"\n"
             << "}\n";
    }
  }

  EmitArtifactEvent("quarantine-complete", artifact.artifact_id, artifact.tab_id,
                    report.summary + " Report: " + artifact.report_path);

  std::string prompt_error;
  const bool released = PromptDownloadRelease(report, artifact.released_path, &prompt_error);
  if (!prompt_error.empty()) {
    EmitArtifactEvent("release-error", artifact.artifact_id, artifact.tab_id, prompt_error);
    return;
  }

  if (!released) {
    EmitArtifactEvent("retained", artifact.artifact_id, artifact.tab_id,
                      "Artifact remained in quarantine at " + artifact.quarantine_path);
    return;
  }

  std::filesystem::path release_path(artifact.released_path);
  std::error_code move_error;
  const std::filesystem::path release_stem = release_path.parent_path() / release_path.stem();
  const std::string release_extension = release_path.extension().string();
  int suffix = 1;
  while (std::filesystem::exists(release_path, move_error)) {
    release_path = std::filesystem::path(
        release_stem.string() + "-" + std::to_string(suffix++) + release_extension);
  }
  move_error.clear();
  std::filesystem::rename(artifact.quarantine_path, release_path, move_error);
  if (move_error) {
    EmitArtifactEvent("release-error", artifact.artifact_id, artifact.tab_id,
                      "Failed to release artifact from quarantine: " + release_path.string());
    return;
  }

  EmitArtifactEvent("released", artifact.artifact_id, artifact.tab_id,
                    "Released artifact to controlled export path " + release_path.string());
}

bool WebKitGtkBrowserEngine::PromptDownloadRelease(const ArtifactScanReport& report,
                                                   const std::string& released_path,
                                                   std::string* error) const {
  GtkWidget* dialog = gtk_dialog_new();
  if (dialog == nullptr) {
    SetError(error, "Failed to create controlled release dialog.");
    return false;
  }

  gtk_window_set_title(GTK_WINDOW(dialog), "Veyra BlackVault Review");
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  if (state_ != nullptr && state_->window != nullptr) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state_->window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  }

  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget* sheet = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_start(sheet, 20);
  gtk_widget_set_margin_end(sheet, 20);
  gtk_widget_set_margin_top(sheet, 18);
  gtk_widget_set_margin_bottom(sheet, 18);
  gtk_box_pack_start(GTK_BOX(content_area), sheet, TRUE, TRUE, 0);

  GtkWidget* title = gtk_label_new("BlackVault Artifact Review");
  gtk_widget_set_halign(title, GTK_ALIGN_START);
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_box_pack_start(GTK_BOX(sheet), title, FALSE, FALSE, 0);

  auto append_line = [&](const std::string& value) {
    GtkWidget* label = gtk_label_new(value.c_str());
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(sheet), label, FALSE, FALSE, 0);
  };

  append_line("Artifact: " + report.detected_name);
  append_line("Source: " + report.source_url);
  append_line("MIME: " + report.mime_guess);
  append_line("Risk: " + report.risk_level + " (" + std::to_string(report.risk_score) + ")");
  append_line("SHA-256: " + report.sha256);
  append_line("Release Path: " + released_path);
  append_line("Summary: " + report.summary);

  if (!report.heuristics.empty()) {
    std::string heuristic_line = "Heuristics: ";
    for (std::size_t index = 0; index < report.heuristics.size(); ++index) {
      if (index > 0) {
        heuristic_line += ", ";
      }
      heuristic_line += report.heuristics[index];
    }
    append_line(heuristic_line);
  }

  gtk_dialog_add_button(GTK_DIALOG(dialog), "Keep Quarantined", GTK_RESPONSE_CANCEL);
  if (config_.security_policy.download_policy != "airgap_quarantine") {
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Release To Controlled Export", GTK_RESPONSE_ACCEPT);
  }

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_widget_show_all(dialog);
  const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
  return response == GTK_RESPONSE_ACCEPT;
}

void WebKitGtkBrowserEngine::ApplyViewSecurityPolicy(void* web_view_ptr) {
  auto* web_view = WEBKIT_WEB_VIEW(web_view_ptr);
  if (web_view == nullptr) {
    return;
  }

  const EngineSecurityPolicy& policy = config_.security_policy;
  WebKitSettings* settings = webkit_settings_new();
  webkit_settings_set_enable_javascript(settings, policy.enable_javascript);
  webkit_settings_set_enable_javascript_markup(settings, policy.enable_javascript_markup);
  webkit_settings_set_enable_html5_local_storage(settings, policy.enable_html5_local_storage);
  webkit_settings_set_enable_html5_database(settings, policy.enable_html5_database);
  webkit_settings_set_enable_media_stream(settings, policy.enable_media_stream);
  webkit_settings_set_javascript_can_access_clipboard(
      settings, policy.enable_javascript_clipboard);
  webkit_settings_set_media_playback_requires_user_gesture(
      settings, policy.media_playback_requires_user_gesture);
  webkit_settings_set_enable_page_cache(settings, policy.enable_page_cache);
  webkit_settings_set_allow_file_access_from_file_urls(
      settings, policy.allow_file_access_from_file_urls);
  webkit_settings_set_allow_universal_access_from_file_urls(
      settings, policy.allow_universal_access_from_file_urls);
  webkit_settings_set_user_agent_with_application_details(settings, "ApexForgeVeyra", "0.7");

  // Extension policy: mixed-content control.
  // The allow_running/displaying_insecure_content toggles were deprecated in
  // WebKitGTK 2.38 and removed in 2.50. On those builds, WebKit blocks active
  // mixed content by default, which already satisfies block_mixed_content=true.
  // On older builds we still honour the explicit toggle.
#if !WEBKIT_CHECK_VERSION(2, 50, 0)
  if (policy.block_mixed_content) {
    webkit_settings_set_allow_running_insecure_content(settings, FALSE);
    webkit_settings_set_allow_displaying_insecure_content(settings, FALSE);
  } else {
    webkit_settings_set_allow_running_insecure_content(settings, TRUE);
    webkit_settings_set_allow_displaying_insecure_content(settings, TRUE);
  }
#else
  (void)policy.block_mixed_content;  // Enforced by WebKit's secure defaults.
#endif

  webkit_web_view_set_settings(web_view, settings);
  g_object_unref(settings);
}

void WebKitGtkBrowserEngine::DispatchDashboardAction(const std::string& action_json) const {
  if (config_.on_dashboard_action) {
    config_.on_dashboard_action(action_json);
  }
}

void WebKitGtkBrowserEngine::PushDashboardState(const std::string& state_json) const {
  if (state_ == nullptr || state_->dashboard_view == nullptr) {
    return;
  }
  // Call window.__VEYRA_UPDATE__(newState) in the dashboard WebView to push
  // refreshed state after a shell action completes (route switch, tool invoke, etc.)
  const std::string js = "if(window.__VEYRA_UPDATE__){window.__VEYRA_UPDATE__(" +
                         state_json + ");}";
  webkit_web_view_evaluate_javascript(
      state_->dashboard_view,
      js.c_str(),
      static_cast<gssize>(js.size()),
      nullptr, nullptr, nullptr, nullptr, nullptr);
}

bool WebKitGtkBrowserEngine::IsThirdPartyFrameBlocked(const std::string& tab_id,
                                                       const std::string& dest_uri,
                                                       std::string* message) const {
  if (config_.security_policy.allow_third_party_frames) {
    return false;
  }

  GUri* dest_parsed = g_uri_parse(dest_uri.c_str(), G_URI_FLAGS_NONE, nullptr);
  if (dest_parsed == nullptr) {
    return false;
  }
  const gchar* dest_host_raw = g_uri_get_host(dest_parsed);
  const std::string dest_host =
      dest_host_raw == nullptr ? std::string() : ToLowerAscii(std::string(dest_host_raw));
  g_uri_unref(dest_parsed);

  if (dest_host.empty()) {
    return false;
  }

  const std::string current_url = GetCurrentUrl(tab_id);
  if (current_url.empty()) {
    return false;
  }

  GUri* top_parsed = g_uri_parse(current_url.c_str(), G_URI_FLAGS_NONE, nullptr);
  if (top_parsed == nullptr) {
    return false;
  }
  const gchar* top_host_raw = g_uri_get_host(top_parsed);
  const std::string top_host =
      top_host_raw == nullptr ? std::string() : ToLowerAscii(std::string(top_host_raw));
  g_uri_unref(top_parsed);

  if (top_host.empty() || dest_host == top_host) {
    return false;
  }

  // Extract effective registrable domain (last two dot-separated labels).
  auto effective_domain = [](const std::string& host) -> std::string {
    const std::size_t last_dot = host.rfind('.');
    if (last_dot == std::string::npos) {
      return host;
    }
    const std::size_t prev_dot = host.rfind('.', last_dot - 1);
    if (prev_dot == std::string::npos) {
      return host;
    }
    return host.substr(prev_dot + 1);
  };

  const std::string dest_domain = effective_domain(dest_host);
  const std::string top_domain = effective_domain(top_host);

  if (dest_domain == top_domain) {
    return false;
  }

  if (message != nullptr) {
    *message = "Blocked third-party frame from '" + dest_host +
               "' under extension policy '" +
               config_.security_policy.extension_policy_id + "'.";
  }
  return true;
}

void WebKitGtkBrowserEngine::InjectExtensionPolicyScript(void* web_view_ptr) const {
  const std::string& script = config_.security_policy.eval_block_script;
  if (script.empty()) {
    return;
  }

  auto* view = WEBKIT_WEB_VIEW(web_view_ptr);
  if (view == nullptr) {
    return;
  }

  WebKitUserContentManager* ucm = webkit_web_view_get_user_content_manager(view);
  if (ucm == nullptr) {
    return;
  }

  WebKitUserScript* user_script = webkit_user_script_new(
      script.c_str(),
      WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
      WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
      nullptr,
      nullptr);
  webkit_user_content_manager_add_script(ucm, user_script);
  webkit_user_script_unref(user_script);
}

void WebKitGtkBrowserEngine::InjectExtensionContentScripts(void* web_view_ptr) const {
  if (config_.extension_content_scripts.empty()) {
    return;
  }
  auto* view = WEBKIT_WEB_VIEW(web_view_ptr);
  if (view == nullptr) {
    return;
  }
  WebKitUserContentManager* ucm = webkit_web_view_get_user_content_manager(view);
  if (ucm == nullptr) {
    return;
  }
  // Loaded browser extensions (e.g. browser-control) inject their content
  // scripts at document-start in the top frame of every browsing tab.
  for (const std::string& body : config_.extension_content_scripts) {
    if (body.empty()) {
      continue;
    }
    WebKitUserScript* user_script = webkit_user_script_new(
        body.c_str(),
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr,
        nullptr);
    webkit_user_content_manager_add_script(ucm, user_script);
    webkit_user_script_unref(user_script);
  }
}

void WebKitGtkBrowserEngine::InjectFingerprintScript(void* web_view_ptr) const {
  if (config_.fingerprint_script.empty()) {
    return;
  }

  auto* view = WEBKIT_WEB_VIEW(web_view_ptr);
  if (view == nullptr) {
    return;
  }

  WebKitUserContentManager* ucm = webkit_web_view_get_user_content_manager(view);
  if (ucm == nullptr) {
    return;
  }

  WebKitUserScript* script = webkit_user_script_new(
      config_.fingerprint_script.c_str(),
      WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
      WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
      nullptr,
      nullptr);
  webkit_user_content_manager_add_script(ucm, script);
  webkit_user_script_unref(script);
}

const PermissionEvaluation* WebKitGtkBrowserEngine::FindPermissionEvaluation(
    PermissionKind permission) const {
  for (const PermissionEvaluation& evaluation : config_.permission_report) {
    if (evaluation.permission == permission) {
      return &evaluation;
    }
  }
  return nullptr;
}

std::string WebKitGtkBrowserEngine::ResolvePromptOrigin(const std::string& tab_id) const {
  return ExtractOriginForPrompt(GetCurrentUrl(tab_id));
}

bool WebKitGtkBrowserEngine::FindCachedPromptDecision(const std::string& origin,
                                                      PermissionKind permission,
                                                      bool* allowed) const {
  if (state_ == nullptr) {
    return false;
  }

  const auto found =
      state_->permission_session_decisions.find(BuildPromptCacheKey(origin, permission));
  if (found == state_->permission_session_decisions.end()) {
    return false;
  }

  if (allowed != nullptr) {
    *allowed = found->second;
  }
  return true;
}

void WebKitGtkBrowserEngine::CachePromptDecision(const std::string& origin,
                                                 PermissionKind permission,
                                                 bool allowed) const {
  if (state_ == nullptr || origin.empty()) {
    return;
  }

  state_->permission_session_decisions[BuildPromptCacheKey(origin, permission)] = allowed;
  std::string flush_error;
  if (!FlushPromptDecisionStore(&flush_error)) {
    EmitPolicyEvent(state_->active_tab_id.empty() ? std::string("system") : state_->active_tab_id,
                    "prompt-store", flush_error);
  }
}

bool WebKitGtkBrowserEngine::LoadPromptDecisionStore(std::string* error) const {
  if (state_ == nullptr) {
    return true;
  }

  state_->permission_session_decisions.clear();
  const std::string& store_path = config_.security_policy.prompt_decision_store_path;
  if (store_path.empty()) {
    return true;
  }

  std::ifstream input(store_path);
  if (!input.is_open()) {
    if (!std::filesystem::exists(store_path)) {
      return true;
    }
    SetError(error, "Failed to open permission decision store: " + store_path);
    return false;
  }

  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }

    std::string origin;
    PermissionKind permission = PermissionKind::kNotifications;
    bool allowed = false;
    if (!ParseStoredPermissionDecision(line, &origin, &permission, &allowed)) {
      continue;
    }
    state_->permission_session_decisions[BuildPromptCacheKey(origin, permission)] = allowed;
  }

  if (!input.good() && !input.eof()) {
    SetError(error, "Failed while reading permission decision store: " + store_path);
    return false;
  }

  return true;
}

bool WebKitGtkBrowserEngine::FlushPromptDecisionStore(std::string* error) const {
  if (state_ == nullptr) {
    return true;
  }

  const std::string& store_path = config_.security_policy.prompt_decision_store_path;
  if (store_path.empty()) {
    return true;
  }

  std::error_code directory_error;
  const std::filesystem::path store_file(store_path);
  std::filesystem::create_directories(store_file.parent_path(), directory_error);
  if (directory_error) {
    SetError(error, "Failed to prepare permission decision store directory: " +
                        store_file.parent_path().string());
    return false;
  }

  std::ofstream output(store_path, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    SetError(error, "Failed to open permission decision store for writing: " + store_path);
    return false;
  }

  for (const auto& [key, allowed] : state_->permission_session_decisions) {
    const std::size_t separator = key.rfind("::");
    if (separator == std::string::npos) {
      continue;
    }
    output << key.substr(0, separator) << '\t'
           << key.substr(separator + 2) << '\t'
           << (allowed ? '1' : '0') << '\n';
  }

  if (!output.good()) {
    SetError(error, "Failed to flush permission decision store: " + store_path);
    return false;
  }

  return true;
}

bool WebKitGtkBrowserEngine::PromptPermissionDialog(const std::string& tab_id,
                                                    PermissionKind permission,
                                                    const std::string& label,
                                                    const std::string& request_details,
                                                    const std::string& rationale,
                                                    std::string* message) const {
  const std::string origin = ResolvePromptOrigin(tab_id);
  const std::string security_mode_label =
      config_.security_policy.mode_name.empty() ? std::string("unknown mode")
                                                : config_.security_policy.mode_name;

  bool cached_allowed = false;
  if (FindCachedPromptDecision(origin, permission, &cached_allowed)) {
    if (message != nullptr) {
      *message = std::string(cached_allowed ? "Allowed " : "Denied ") + label +
                 " using cached session decision for " + origin + ".";
    }
    EmitPolicyEvent(tab_id, "prompt",
                    std::string(cached_allowed ? "Reused allow" : "Reused deny") +
                        " decision for " + ToString(permission) + " at " + origin + ".");
    return cached_allowed;
  }

  GtkWidget* dialog = gtk_dialog_new();
  if (dialog == nullptr) {
    if (message != nullptr) {
      *message = "Denied " + label + " because the prompt dialog could not be created.";
    }
    return false;
  }

  gtk_window_set_title(GTK_WINDOW(dialog), "Veyra Permission Sheet");
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  if (state_ != nullptr && state_->window != nullptr) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state_->window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  }

  GtkCssProvider* css_provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      css_provider,
      ".veyra-sheet {"
      "  background: linear-gradient(180deg, #0f1720, #192531);"
      "  border-radius: 20px;"
      "}"
      ".veyra-badge {"
      "  color: #9ad7ff;"
      "  background: rgba(23, 96, 134, 0.45);"
      "  border-radius: 999px;"
      "  padding: 6px 12px;"
      "  font-weight: 700;"
      "}"
      ".veyra-title {"
      "  color: #f4f7fb;"
      "  font-size: 24px;"
      "  font-weight: 800;"
      "}"
      ".veyra-copy {"
      "  color: #ced9e7;"
      "  font-size: 13px;"
      "}"
      ".veyra-card {"
      "  background: rgba(255,255,255,0.05);"
      "  border: 1px solid rgba(154,215,255,0.16);"
      "  border-radius: 14px;"
      "  padding: 14px;"
      "}"
      ".veyra-field-label {"
      "  color: #83a2ba;"
      "  font-size: 11px;"
      "  font-weight: 700;"
      "}"
      ".veyra-field-value {"
      "  color: #f3f7fa;"
      "  font-size: 13px;"
      "  font-weight: 600;"
      "}"
      ".veyra-deny {"
      "  background: #253341;"
      "  color: #eff6ff;"
      "  border-radius: 10px;"
      "  padding: 8px 16px;"
      "}"
      ".veyra-allow {"
      "  background: #2aa6ff;"
      "  color: #07111a;"
      "  border-radius: 10px;"
      "  padding: 8px 16px;"
      "  font-weight: 800;"
      "}",
      -1, nullptr);

  AddCssProviderToWidget(dialog, css_provider);

  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content_area), 0);

  GtkWidget* sheet = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_set_margin_start(sheet, 22);
  gtk_widget_set_margin_end(sheet, 22);
  gtk_widget_set_margin_top(sheet, 20);
  gtk_widget_set_margin_bottom(sheet, 18);
  gtk_style_context_add_class(gtk_widget_get_style_context(sheet), "veyra-sheet");
  AddCssProviderToWidget(sheet, css_provider);
  gtk_box_pack_start(GTK_BOX(content_area), sheet, TRUE, TRUE, 0);

  GtkWidget* badge = gtk_label_new("Veyra Guard");
  gtk_widget_set_halign(badge, GTK_ALIGN_START);
  gtk_style_context_add_class(gtk_widget_get_style_context(badge), "veyra-badge");
  AddCssProviderToWidget(badge, css_provider);
  gtk_box_pack_start(GTK_BOX(sheet), badge, FALSE, FALSE, 0);

  GtkWidget* title = gtk_label_new("Permission Review");
  gtk_widget_set_halign(title, GTK_ALIGN_START);
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_style_context_add_class(gtk_widget_get_style_context(title), "veyra-title");
  AddCssProviderToWidget(title, css_provider);
  gtk_box_pack_start(GTK_BOX(sheet), title, FALSE, FALSE, 0);

  GtkWidget* intro = gtk_label_new(
      ("The site is requesting " + label + ". Review the request before Veyra exposes this surface.")
          .c_str());
  gtk_widget_set_halign(intro, GTK_ALIGN_START);
  gtk_label_set_xalign(GTK_LABEL(intro), 0.0f);
  gtk_label_set_line_wrap(GTK_LABEL(intro), TRUE);
  gtk_style_context_add_class(gtk_widget_get_style_context(intro), "veyra-copy");
  AddCssProviderToWidget(intro, css_provider);
  gtk_box_pack_start(GTK_BOX(sheet), intro, FALSE, FALSE, 0);

  GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_style_context_add_class(gtk_widget_get_style_context(card), "veyra-card");
  AddCssProviderToWidget(card, css_provider);
  gtk_box_pack_start(GTK_BOX(sheet), card, FALSE, FALSE, 0);

  auto append_field = [&](const std::string& field_label, const std::string& field_value) {
    GtkWidget* label_widget = gtk_label_new(field_label.c_str());
    gtk_widget_set_halign(label_widget, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(label_widget), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(label_widget), "veyra-field-label");
    AddCssProviderToWidget(label_widget, css_provider);
    gtk_box_pack_start(GTK_BOX(card), label_widget, FALSE, FALSE, 0);

    GtkWidget* value_widget = gtk_label_new(field_value.c_str());
    gtk_widget_set_halign(value_widget, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(value_widget), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(value_widget), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(value_widget), "veyra-field-value");
    AddCssProviderToWidget(value_widget, css_provider);
    gtk_box_pack_start(GTK_BOX(card), value_widget, FALSE, FALSE, 0);
  };

  append_field("Origin", origin);
  append_field("Security Mode", security_mode_label);
  append_field("Request", request_details);
  append_field("Policy Context", rationale);
  append_field("Decision Scope", "Remember this choice for this origin and permission.");

  GtkWidget* deny_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "Block", GTK_RESPONSE_CANCEL);
  GtkWidget* allow_button =
      gtk_dialog_add_button(GTK_DIALOG(dialog), "Allow For This Session", GTK_RESPONSE_ACCEPT);
  gtk_style_context_add_class(gtk_widget_get_style_context(deny_button), "veyra-deny");
  gtk_style_context_add_class(gtk_widget_get_style_context(allow_button), "veyra-allow");
  AddCssProviderToWidget(deny_button, css_provider);
  AddCssProviderToWidget(allow_button, css_provider);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_widget_show_all(dialog);

  const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  g_object_unref(css_provider);
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }

  const bool allowed = response == GTK_RESPONSE_ACCEPT;
  CachePromptDecision(origin, permission, allowed);
  if (message != nullptr) {
    *message = std::string(allowed ? "Allowed " : "Denied ") + label +
               " after interactive review for " + origin + ".";
  }

  EmitPolicyEvent(tab_id, "prompt",
                  std::string(allowed ? "Operator allowed " : "Operator denied ") +
                      ToString(permission) + " for " + origin + " and cached it for this session.");
  return allowed;
}

bool WebKitGtkBrowserEngine::ResolvePermissionDecision(const std::string& tab_id,
                                                       PermissionKind permission,
                                                       const std::string& label,
                                                       const std::string& request_details,
                                                       std::string* message) const {
  const PermissionEvaluation* evaluation = FindPermissionEvaluation(permission);
  if (evaluation == nullptr) {
    if (message != nullptr) {
      *message = "Denied " + label + " because no policy evaluation was available.";
    }
    return false;
  }

  if (evaluation->decision == PermissionDecision::kAllow) {
    if (message != nullptr) {
      *message = "Allowed " + label + ": " + evaluation->rationale;
    }
    return true;
  }

  if (evaluation->decision == PermissionDecision::kPrompt) {
    return PromptPermissionDialog(tab_id, permission, label, request_details, evaluation->rationale,
                                  message);
  }

  if (message != nullptr) {
    *message = "Denied " + label + ": " + evaluation->rationale;
  }
  return false;
}

bool WebKitGtkBrowserEngine::HandlePermissionRequest(const std::string& tab_id,
                                                     void* permission_request_ptr,
                                                     std::string* message) const {
  auto* permission_request = WEBKIT_PERMISSION_REQUEST(permission_request_ptr);
  if (permission_request == nullptr) {
    *message = "Denied unknown permission request.";
    return false;
  }

  if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(permission_request)) {
    return ResolvePermissionDecision(
        tab_id, PermissionKind::kNotifications, "notification permission",
        "The current page requested notification delivery access.", message);
  }
  if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(permission_request)) {
    return ResolvePermissionDecision(
        tab_id, PermissionKind::kGeolocation, "geolocation permission",
        "The current page requested precise location access.", message);
  }
  if (WEBKIT_IS_CLIPBOARD_PERMISSION_REQUEST(permission_request)) {
    return ResolvePermissionDecision(
        tab_id, PermissionKind::kClipboard, "clipboard permission",
        "The current page requested clipboard access.", message);
  }
  if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(permission_request)) {
    auto* user_media_request = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(permission_request);
    const bool needs_audio =
        webkit_user_media_permission_is_for_audio_device(user_media_request);
    const bool needs_video =
        webkit_user_media_permission_is_for_video_device(user_media_request) ||
        webkit_user_media_permission_is_for_display_device(user_media_request);

    std::vector<std::string> requested_capabilities;
    std::vector<std::string> prompt_rationales;

    auto append_prompt_rationale = [&](PermissionKind permission,
                                       const std::string& capability_label) -> bool {
      requested_capabilities.push_back(capability_label);
      const PermissionEvaluation* evaluation = FindPermissionEvaluation(permission);
      if (evaluation == nullptr) {
        *message = "Denied media permission because no policy evaluation was available for " +
                   capability_label + ".";
        return false;
      }
      if (evaluation->decision == PermissionDecision::kDeny) {
        *message = "Denied media permission for " + capability_label + ": " + evaluation->rationale;
        return false;
      }
      if (evaluation->decision == PermissionDecision::kPrompt) {
        prompt_rationales.push_back(capability_label + ": " + evaluation->rationale);
      }
      return true;
    };

    if (needs_audio && !append_prompt_rationale(PermissionKind::kMicrophone, "microphone")) {
      return false;
    }
    if (needs_video && !append_prompt_rationale(PermissionKind::kCamera, "camera/display")) {
      return false;
    }

    if (prompt_rationales.empty()) {
      *message = "Allowed user-media permission request.";
      return true;
    }

    std::string request_summary;
    for (std::size_t index = 0; index < requested_capabilities.size(); ++index) {
      if (index > 0) {
        request_summary += index + 1 == requested_capabilities.size() ? " and " : ", ";
      }
      request_summary += requested_capabilities[index];
    }

    std::string rationale_summary;
    for (std::size_t index = 0; index < prompt_rationales.size(); ++index) {
      if (index > 0) {
        rationale_summary += " | ";
      }
      rationale_summary += prompt_rationales[index];
    }

    const PermissionKind prompt_permission =
        needs_video ? PermissionKind::kCamera : PermissionKind::kMicrophone;
    const bool allowed = PromptPermissionDialog(
        tab_id, prompt_permission, "media capture permission",
        "The current page requested access to " + request_summary + ".",
        rationale_summary, message);
    const std::string origin = ResolvePromptOrigin(tab_id);
    if (needs_audio) {
      CachePromptDecision(origin, PermissionKind::kMicrophone, allowed);
    }
    if (needs_video) {
      CachePromptDecision(origin, PermissionKind::kCamera, allowed);
    }
    return allowed;
  }
  if (WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(permission_request)) {
    *message = "Denied device-info permission request under Veyra Phase 3 policy.";
    return false;
  }

  *message = "Denied unsupported permission request type for tab " + tab_id + ".";
  return false;
}

bool WebKitGtkBrowserEngine::ShouldAllowNavigation(const std::string& tab_id,
                                                   const std::string& uri,
                                                   std::string* message) const {
  const EngineSecurityPolicy& policy = config_.security_policy;

  if (!IsValidUri(uri)) {
    *message = "Blocked navigation because URI was invalid: " + uri;
    return false;
  }

  GUri* parsed_uri = g_uri_parse(uri.c_str(), G_URI_FLAGS_NONE, nullptr);
  if (parsed_uri == nullptr) {
    *message = "Blocked navigation because URI parsing failed: " + uri;
    return false;
  }

  const gchar* scheme_value = g_uri_get_scheme(parsed_uri);
  const gchar* host_value = g_uri_get_host(parsed_uri);
  const std::string scheme = scheme_value == nullptr ? std::string() : ToLowerAscii(std::string(scheme_value));
  const std::string host = host_value == nullptr ? std::string() : ToLowerAscii(std::string(host_value));

  const bool is_web_scheme = scheme == "http" || scheme == "https" || scheme == "about";
  const bool is_ip_literal = !host.empty() && g_hostname_is_ip_address(host.c_str());
  const bool is_localhost = IsLocalhostHost(host);
  const bool is_private_network =
      IsPrivateIpv4(host) || IsPrivateIpv6(host) || EndsWith(host, ".local");
  const bool is_i2p_eepsite = EndsWith(host, ".i2p");

  const auto cleanup_uri = [&parsed_uri]() {
    if (parsed_uri != nullptr) {
      g_uri_unref(parsed_uri);
      parsed_uri = nullptr;
    }
  };

  if (!policy.allow_non_web_schemes && !is_web_scheme) {
    *message = "Blocked non-web scheme navigation for " + tab_id + ": " + uri;
    cleanup_uri();
    return false;
  }
  if (!policy.allow_ip_literal_navigation && is_ip_literal) {
    *message = "Blocked direct IP navigation under DNS leak-prevention policy: " + uri;
    cleanup_uri();
    return false;
  }
  if (!policy.allow_localhost_navigation && is_localhost) {
    *message = "Blocked localhost navigation under isolation policy: " + uri;
    cleanup_uri();
    return false;
  }
  if (!policy.allow_private_network_navigation && is_private_network) {
    *message = "Blocked private-network navigation under isolation policy: " + uri;
    cleanup_uri();
    return false;
  }
  if (policy.javascript_policy != "allow" && scheme == "javascript") {
    *message = "Blocked javascript: navigation under restricted JavaScript policy.";
    cleanup_uri();
    return false;
  }

  if (is_i2p_eepsite && !policy.is_i2p_route) {
    *message = "Blocked .i2p eepsite navigation: I2P route is not active for this persona.";
    cleanup_uri();
    return false;
  }

  if (is_i2p_eepsite && scheme == "https") {
    *message = "Blocked HTTPS to .i2p eepsite: I2P eepsites are HTTP-only.";
    cleanup_uri();
    return false;
  }

  // Extension policy: block navigation to domains on the persona's blocked list.
  // Subdomain matching is handled by IsBlockedDomain (e.g., ads.doubleclick.net
  // matches the "doubleclick.net" rule).
  if (!host.empty() && !policy.blocked_domains.empty() &&
      IsBlockedDomain(policy.blocked_domains, host)) {
    *message = "Blocked navigation to '" + host +
               "' by extension policy '" + policy.extension_policy_id + "'.";
    cleanup_uri();
    return false;
  }

  cleanup_uri();
  return true;
}

void WebKitGtkBrowserEngine::EmitPolicyEvent(const std::string& tab_id,
                                             const std::string& category,
                                             const std::string& message) const {
  if (config_.on_policy_event) {
    config_.on_policy_event(tab_id, category, message);
  }
}

}  // namespace veyra
