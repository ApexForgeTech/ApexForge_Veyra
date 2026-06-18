package com.apexforge.veyra;

import android.app.AlertDialog;
import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.webkit.CookieManager;
import android.webkit.DownloadListener;
import android.webkit.JavascriptInterface;
import android.webkit.PermissionRequest;
import android.webkit.URLUtil;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URLEncoder;

public class MainActivity extends AppCompatActivity {

    private WebView mUiWebView;
    private WebView mContentWebView;
    private FrameLayout mSidebarContainer;
    private EditText mAddressBar;
    private ProgressBar mPageProgress;
    private Button mBackButton;
    private Button mForwardButton;
    private boolean isPanelOpen = false;

    private static final String PREFS = "veyra_prefs";
    private SharedPreferences mPrefs;
    private ValueCallback<Uri[]> mFileCallback;
    private ActivityResultLauncher<Intent> mFileChooser;

    // Security profile + extension content script (parity with desktop).
    private VeyraProfiles.Persona mPersona;       // fallback when native core is unavailable
    private String mControlScript = "";           // extensions/browser-control/control.js
    private boolean mNativeReady = false;         // C++ core (schemas) loaded?
    private String mActivePersonaId = "real_identity";

    static {
        System.loadLibrary("veyra_native");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window window = getWindow();
        window.setStatusBarColor(0xFF05080A);
        window.setNavigationBarColor(0xFF05080A);
        setContentView(R.layout.activity_main);

        mPrefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        mActivePersonaId = mPrefs.getString("persona", "real_identity");
        mPersona = VeyraProfiles.byId(mActivePersonaId);
        mControlScript = loadAsset("extensions/browser-control/control.js");

        // Bring up the shared C++ core: extract the schema/seed tree to a real
        // filesystem path (assets aren't files) and bootstrap the same engine
        // the desktop uses, so personas/policy/fingerprint are engine-driven.
        try {
            String root = getFilesDir().getAbsolutePath();
            extractAsset("schemas", new java.io.File(root, "schemas"));
            mNativeReady = nativeInit(root);
        } catch (Throwable t) {
            mNativeReady = false;
        }

        // Persist cookies across sessions (the "data not saved" fix).
        CookieManager.getInstance().setAcceptCookie(true);

        // File uploads (<input type=file>) via the system picker.
        mFileChooser = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(), result -> {
                    if (mFileCallback == null) return;
                    Uri[] uris = null;
                    if (result.getResultCode() == RESULT_OK && result.getData() != null
                            && result.getData().getData() != null) {
                        uris = new Uri[]{result.getData().getData()};
                    }
                    mFileCallback.onReceiveValue(uris);
                    mFileCallback = null;
                });

        mSidebarContainer = findViewById(R.id.sidebar_container);
        mAddressBar = findViewById(R.id.address_bar);
        mPageProgress = findViewById(R.id.page_progress);
        mBackButton = findViewById(R.id.btn_back);
        mForwardButton = findViewById(R.id.btn_forward);
        Button reloadButton = findViewById(R.id.btn_reload);
        Button closePanelButton = findViewById(R.id.btn_close_panel);
        FloatingActionButton toggleBtn = findViewById(R.id.fab_toggle_panel);
        mSidebarContainer.setVisibility(View.GONE);
        mSidebarContainer.setTranslationX(-getResources().getDisplayMetrics().density * 24);
        mSidebarContainer.setAlpha(0f);

        // 1. Setup Security Panel (Sidebar) — the React dashboard, fed a live
        //    __VEYRA_STATE__ and a postMessage bridge so it shows real profile
        //    data and can drive the browser (parity with the desktop shell).
        mUiWebView = findViewById(R.id.ui_webview);
        setupWebView(mUiWebView);
        mUiWebView.addJavascriptInterface(new DashboardHost(), "VeyraHost");
        mUiWebView.loadUrl("file:///android_asset/ui/index.html");

        // 2. Setup Main Browser
        mContentWebView = findViewById(R.id.content_webview);
        setupWebView(mContentWebView);
        enhanceContentWebView(mContentWebView);
        mContentWebView.addJavascriptInterface(new VeyraBridge(), "Veyra");
        setupBrowserChrome();
        String launchUrl = getIntentUrl(getIntent());
        mContentWebView.loadUrl(launchUrl == null ? "file:///android_asset/start.html" : launchUrl);

        // Long-press reload → browser menu (find, downloads, desktop site,
        // share, settings) — keeps the slim toolbar uncluttered.
        reloadButton.setOnLongClickListener(v -> { showBrowserMenu(v); return true; });

        // 3. Toggle Logic — tap = security panel, long-press = persona switcher.
        toggleBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (isPanelOpen) {
                    closePanel();
                } else {
                    openPanel();
                }
            }
        });
        toggleBtn.setOnLongClickListener(v -> { showPersonaSwitcher(); return true; });
        closePanelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                closePanel();
            }
        });
        reloadButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mContentWebView.reload();
            }
        });
        mBackButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mContentWebView.canGoBack()) {
                    mContentWebView.goBack();
                }
            }
        });
        mForwardButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mContentWebView.canGoForward()) {
                    mContentWebView.goForward();
                }
            }
        });

        // 4. Init C++ Backend
        initNativeBackend();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        String url = getIntentUrl(intent);
        if (url != null) {
            mContentWebView.loadUrl(url);
        }
    }

    @Override
    public void onBackPressed() {
        if (isPanelOpen) {
            closePanel();
        } else if (mContentWebView != null && mContentWebView.canGoBack()) {
            mContentWebView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    private void setupWebView(WebView webView) {
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        // Persistent client-side storage so sites keep logins/state between runs.
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);
        // Touch: pinch-to-zoom without on-screen zoom buttons.
        settings.setSupportZoom(true);
        settings.setBuiltInZoomControls(true);
        settings.setDisplayZoomControls(false);
        settings.setMediaPlaybackRequiresUserGesture(true);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                if (view != mContentWebView || mPageProgress == null) {
                    return;
                }
                mPageProgress.setProgress(newProgress);
                mPageProgress.setVisibility(newProgress >= 100 ? View.GONE : View.VISIBLE);
            }
        });
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, android.graphics.Bitmap favicon) {
                if (view == mContentWebView) {
                    updateAddressBar(url);
                    updateNavigationButtons();
                    // Inject persona fingerprint defense + the browser-control
                    // extension as early as possible (document-start parity).
                    injectContentScripts(view);
                }
                super.onPageStarted(view, url, favicon);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                if (view == mContentWebView) {
                    updateAddressBar(url);
                    updateNavigationButtons();
                    injectContentScripts(view);  // ensure present after full load
                } else if (view == mUiWebView) {
                    injectDashboardState();      // feed the React panel
                }
                super.onPageFinished(view, url);
            }
        });
    }

    /** Injects the per-persona fingerprint defence + extension policy + control
     *  surface. Fingerprint/eval-block come from the real C++ engine when ready. */
    private void injectContentScripts(WebView view) {
        // 1. Fingerprint defence (engine-driven; falls back to the Java model).
        String fp = mNativeReady ? safeNative(() -> nativeFingerprintScript(mActivePersonaId)) : null;
        if (fp == null || fp.isEmpty()) {
            fp = mPersona != null ? VeyraProfiles.fingerprintScript(mPersona) : "";
        }
        if (fp != null && !fp.isEmpty()) view.evaluateJavascript(fp, null);

        // 2. Extension policy: block eval()/Function() when the persona forbids it.
        if (mNativeReady) {
            String evalBlock = safeNative(() -> nativeExtensionEvalBlock(mActivePersonaId));
            if (evalBlock != null && !evalBlock.isEmpty()) view.evaluateJavascript(evalBlock, null);
        }

        // 3. The browser-control extension content script (desktop parity).
        if (mControlScript != null && !mControlScript.isEmpty()) {
            view.evaluateJavascript(mControlScript, null);
        }
    }

    /** Pushes a live VeyraState + a webkit.messageHandlers shim into the panel. */
    private void injectDashboardState() {
        if (mUiWebView == null) return;
        String state = mNativeReady ? safeNative(() -> nativePersonaState(mActivePersonaId)) : null;
        if (state == null || state.length() < 3) {
            state = VeyraProfiles.buildStateJson(mActivePersonaId, "1.0-mobile");
        }
        String js =
            "(function(){window.__VEYRA_STATE__=" + state + ";"
            + "if(!window.webkit)window.webkit={};"
            + "if(!window.webkit.messageHandlers)window.webkit.messageHandlers={};"
            + "window.webkit.messageHandlers.veyra={postMessage:function(m){"
            + "try{VeyraHost.postAction(typeof m==='string'?m:JSON.stringify(m));}catch(e){}}};"
            + "if(typeof window.__VEYRA_UPDATE__==='function'){window.__VEYRA_UPDATE__(window.__VEYRA_STATE__);}"
            + "})();";
        mUiWebView.evaluateJavascript(js, null);
    }

    private void setupBrowserChrome() {
        mAddressBar.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                boolean enterPressed = event != null
                        && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                        && event.getAction() == KeyEvent.ACTION_UP;
                if (actionId == EditorInfo.IME_ACTION_GO || enterPressed) {
                    navigateFromAddress(v.getText().toString());
                    mAddressBar.clearFocus();
                    return true;
                }
                return false;
            }
        });
    }

    private void navigateFromAddress(String rawValue) {
        String value = rawValue == null ? "" : rawValue.trim();
        if (value.isEmpty()) {
            return;
        }

        if (value.startsWith("veyra:") || value.startsWith("about:") || value.startsWith("file:")) {
            mContentWebView.loadUrl(value);
        } else if (value.indexOf(' ') < 0 && value.indexOf('.') > 0) {
            mContentWebView.loadUrl(value.contains("://") ? value : "https://" + value);
        } else {
            mContentWebView.loadUrl(buildSearchUrl(value));
        }
    }

    private void updateAddressBar(String url) {
        if (mAddressBar == null || mAddressBar.hasFocus()) {
            return;
        }
        if (url == null || url.startsWith("file:///android_asset/start.html")) {
            mAddressBar.setText("");
            mAddressBar.setHint("Search or enter address");
        } else {
            mAddressBar.setText(url);
        }
    }

    private void updateNavigationButtons() {
        if (mBackButton == null || mForwardButton == null || mContentWebView == null) {
            return;
        }
        mBackButton.setEnabled(mContentWebView.canGoBack());
        mForwardButton.setEnabled(mContentWebView.canGoForward());
        mBackButton.setAlpha(mContentWebView.canGoBack() ? 1f : 0.42f);
        mForwardButton.setAlpha(mContentWebView.canGoForward() ? 1f : 0.42f);
    }

    private String getIntentUrl(Intent intent) {
        if (intent == null || intent.getData() == null) {
            return null;
        }
        Uri uri = intent.getData();
        String scheme = uri.getScheme();
        if ("http".equals(scheme) || "https".equals(scheme)) {
            return uri.toString();
        }
        return null;
    }

    private void openPanel() {
        isPanelOpen = true;
        mSidebarContainer.setVisibility(View.VISIBLE);
        mSidebarContainer.animate()
                .alpha(1f)
                .translationX(0f)
                .setDuration(180)
                .start();
    }

    private void closePanel() {
        isPanelOpen = false;
        float offset = -getResources().getDisplayMetrics().density * 24;
        mSidebarContainer.animate()
                .alpha(0f)
                .translationX(offset)
                .setDuration(160)
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        if (!isPanelOpen) {
                            mSidebarContainer.setVisibility(View.GONE);
                        }
                    }
                })
                .start();
    }

    // User-Agent strategy (matches desktop): hardened/ephemeral personas send a
    // uniform "blend-in" UA (anti-fingerprint, Mullvad-style); others a Chrome
    // UA branded "Veyra/1.0" so sites show Veyra, not just the WebView default.
    private void applyUserAgent(WebView web) {
        boolean hardened = mPersona != null && mPersona.ephemeral;
        String ua = hardened
                ? "Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) "
                        + "Chrome/124.0.0.0 Mobile Safari/537.36"
                : web.getSettings().getUserAgentString() + " Veyra/1.0";
        web.getSettings().setUserAgentString(ua);
    }

    // ── Real-browser capabilities on the content WebView ────────────
    private void enhanceContentWebView(final WebView web) {
        // Per-WebView third-party cookie acceptance (needed by many logins).
        CookieManager.getInstance().setAcceptThirdPartyCookies(web, true);
        applyUserAgent(web);

        // Downloads → system DownloadManager → public Downloads folder.
        web.setDownloadListener(new DownloadListener() {
            @Override
            public void onDownloadStart(String url, String userAgent, String contentDisposition,
                                        String mimeType, long contentLength) {
                try {
                    DownloadManager.Request req = new DownloadManager.Request(Uri.parse(url));
                    String name = URLUtil.guessFileName(url, contentDisposition, mimeType);
                    req.setMimeType(mimeType);
                    req.setTitle(name);
                    req.addRequestHeader("User-Agent", userAgent);
                    req.setNotificationVisibility(
                            DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
                    req.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, name);
                    DownloadManager dm = (DownloadManager) getSystemService(Context.DOWNLOAD_SERVICE);
                    if (dm != null) {
                        dm.enqueue(req);
                        Toast.makeText(MainActivity.this, "Downloading " + name, Toast.LENGTH_SHORT).show();
                    }
                } catch (Exception e) {
                    Toast.makeText(MainActivity.this, "Download failed", Toast.LENGTH_SHORT).show();
                }
            }
        });

        // Content chrome client: progress + file upload + permission prompts.
        web.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                if (mPageProgress == null) return;
                mPageProgress.setProgress(newProgress);
                mPageProgress.setVisibility(newProgress >= 100 ? View.GONE : View.VISIBLE);
            }
            @Override
            public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback,
                                             FileChooserParams params) {
                mFileCallback = callback;
                try {
                    mFileChooser.launch(params.createIntent());
                    return true;
                } catch (Exception e) {
                    mFileCallback = null;
                    return false;
                }
            }
            @Override
            public void onPermissionRequest(final PermissionRequest request) {
                runOnUiThread(() -> new AlertDialog.Builder(MainActivity.this)
                        .setTitle("Permission request")
                        .setMessage(String.join("\n", request.getResources()))
                        .setPositiveButton("Allow", (d, w) -> request.grant(request.getResources()))
                        .setNegativeButton("Block", (d, w) -> request.deny())
                        .setOnCancelListener(d -> request.deny())
                        .show());
            }
            @Override
            public void onGeolocationPermissionsShowPrompt(String origin,
                    android.webkit.GeolocationPermissions.Callback callback) {
                new AlertDialog.Builder(MainActivity.this)
                        .setTitle("Location request")
                        .setMessage(origin + " wants your location")
                        .setPositiveButton("Allow", (d, w) -> callback.invoke(origin, true, false))
                        .setNegativeButton("Block", (d, w) -> callback.invoke(origin, false, false))
                        .show();
            }
        });
    }

    // ── Browser menu (long-press reload) ────────────────────────────
    private void showBrowserMenu(View anchor) {
        final String[] items = {"Find in page", "Downloads", "Desktop site", "Share", "Settings"};
        new AlertDialog.Builder(this)
                .setItems(items, (d, which) -> {
                    switch (which) {
                        case 0: showFindDialog(); break;
                        case 1:
                            try { startActivity(new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS)); }
                            catch (Exception ignored) {}
                            break;
                        case 2: toggleDesktopSite(); break;
                        case 3: shareCurrent(); break;
                        case 4: showSettings(); break;
                    }
                })
                .show();
    }

    private void showFindDialog() {
        final EditText input = new EditText(this);
        input.setHint("Find in page");
        input.setTextColor(0xFFEEF6FF);
        final AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("Find in page")
                .setView(input)
                .setPositiveButton("Next", (d, w) -> mContentWebView.findNext(true))
                .setNegativeButton("Done", (d, w) -> mContentWebView.clearMatches())
                .create();
        input.addTextChangedListener(new android.text.TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            public void onTextChanged(CharSequence s, int a, int b, int c) {
                mContentWebView.findAllAsync(s.toString());
            }
            public void afterTextChanged(android.text.Editable s) {}
        });
        dialog.show();
    }

    private void toggleDesktopSite() {
        WebSettings s = mContentWebView.getSettings();
        boolean mobile = s.getUserAgentString() == null || !s.getUserAgentString().contains("X11");
        s.setUserAgentString(mobile
                ? "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120 Safari/537.36"
                : null);
        s.setUseWideViewPort(true);
        s.setLoadWithOverviewMode(true);
        mContentWebView.reload();
    }

    private void shareCurrent() {
        String url = mContentWebView.getUrl();
        if (url == null) return;
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_TEXT, url);
        startActivity(Intent.createChooser(i, "Share link"));
    }

    private void showSettings() {
        final String[] labels = {"DuckDuckGo", "Google", "Bing", "Brave Search"};
        final String[] ids = {"duckduckgo", "google", "bing", "brave"};
        String curId = mPrefs.getString("search_engine", "duckduckgo");
        int sel = 0;
        for (int i = 0; i < ids.length; i++) if (ids[i].equals(curId)) sel = i;
        new AlertDialog.Builder(this)
                .setTitle("Search engine")
                .setSingleChoiceItems(labels, sel, (d, which) ->
                        mPrefs.edit().putString("search_engine", ids[which]).apply())
                .setPositiveButton("Done", null)
                .setNeutralButton("Clear browsing data", (d, w) -> clearBrowsingData())
                .show();
    }

    private void clearBrowsingData() {
        CookieManager.getInstance().removeAllCookies(null);
        CookieManager.getInstance().flush();
        WebStorage.getInstance().deleteAllData();
        if (mContentWebView != null) {
            mContentWebView.clearCache(true);
            mContentWebView.clearHistory();
            mContentWebView.clearFormData();
        }
        Toast.makeText(this, "Browsing data cleared", Toast.LENGTH_SHORT).show();
    }

    // Flush cookies/storage to disk so nothing is lost when leaving the app.
    @Override
    protected void onPause() {
        super.onPause();
        CookieManager.getInstance().flush();
    }

    @Override
    protected void onDestroy() {
        CookieManager.getInstance().flush();
        super.onDestroy();
    }

    public class VeyraBridge {
        @JavascriptInterface
        public String searchUrl(String query) {
            return MainActivity.this.buildSearchUrl(query);
        }
    }

    /** Bridge for the React control panel (sidebar) → native actions. */
    public class DashboardHost {
        @JavascriptInterface
        public void postAction(String json) {
            runOnUiThread(() -> handleDashboardAction(json));
        }
    }

    private void handleDashboardAction(String json) {
        try {
            JSONObject o = new JSONObject(json);
            String action = o.optString("action", "");
            switch (action) {
                case "open_tab":
                case "navigate": {
                    String url = o.optString("url", "");
                    if (!url.isEmpty()) { mContentWebView.loadUrl(url); closePanel(); }
                    break;
                }
                case "switch_route": {
                    String rp = o.optString("route_profile_id", "");
                    Toast.makeText(this, "Route → " + rp + " (needs local router on mobile)",
                            Toast.LENGTH_SHORT).show();
                    break;
                }
                case "request_state_refresh":
                    injectDashboardState();
                    break;
                default:
                    break;
            }
        } catch (Exception ignored) {}
    }

    private String buildSearchUrl(String query) {
        String engine = mPrefs != null ? mPrefs.getString("search_engine", "duckduckgo") : "duckduckgo";
        String base;
        switch (engine) {
            case "google": base = "https://www.google.com/search?q="; break;
            case "bing":   base = "https://www.bing.com/search?q="; break;
            case "brave":  base = "https://search.brave.com/search?q="; break;
            default:        base = "https://duckduckgo.com/?q=";
        }
        try { return base + URLEncoder.encode(query, "UTF-8"); }
        catch (Exception e) { return base + Uri.encode(query); }
    }

    // ── Persona switcher (security profiles, engine-driven) ─────────
    private void showPersonaSwitcher() {
        String[] ids;
        String[] labels;
        try {
            org.json.JSONArray arr = new org.json.JSONArray(
                    mNativeReady ? safeNative(this::nativeListPersonasSafe) : "[]");
            if (arr.length() == 0) throw new Exception("empty");
            ids = new String[arr.length()];
            labels = new String[arr.length()];
            for (int i = 0; i < arr.length(); i++) {
                JSONObject p = arr.getJSONObject(i);
                ids[i] = p.optString("id");
                labels[i] = p.optString("display_name") + "  ·  "
                        + p.optString("security_mode")
                        + (p.optBoolean("ephemeral") ? "  (ephemeral)" : "");
            }
        } catch (Exception e) {
            ids = new String[VeyraProfiles.PERSONAS.length];
            labels = new String[VeyraProfiles.PERSONAS.length];
            for (int i = 0; i < VeyraProfiles.PERSONAS.length; i++) {
                ids[i] = VeyraProfiles.PERSONAS[i].id;
                labels[i] = VeyraProfiles.PERSONAS[i].displayName + "  ·  "
                        + VeyraProfiles.PERSONAS[i].securityMode
                        + (VeyraProfiles.PERSONAS[i].ephemeral ? "  (ephemeral)" : "");
            }
        }
        final String[] fIds = ids;
        int sel = 0;
        for (int i = 0; i < fIds.length; i++) if (fIds[i].equals(mActivePersonaId)) sel = i;
        new AlertDialog.Builder(this)
                .setTitle("Security profile")
                .setSingleChoiceItems(labels, sel, (d, which) -> {
                    applyPersona(fIds[which]);
                    d.dismiss();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    /** Switches the active persona and applies its security model to the WebView. */
    private void applyPersona(String personaId) {
        if (personaId == null || personaId.equals(mActivePersonaId)) return;
        mActivePersonaId = personaId;
        mPersona = VeyraProfiles.byId(personaId);
        mPrefs.edit().putString("persona", personaId).apply();

        // Apply engine-computed security flags (ephemeral storage, cookie policy).
        boolean ephemeral = mPersona.ephemeral;
        boolean acceptThirdParty = true;
        try {
            if (mNativeReady) {
                JSONObject flags = new JSONObject(safeNative(() -> nativeSecurityFlags(personaId)));
                ephemeral = flags.optBoolean("ephemeral", ephemeral);
                acceptThirdParty = flags.optBoolean("allow_third_party_frames", true);
            }
        } catch (Exception ignored) {}

        CookieManager cm = CookieManager.getInstance();
        if (ephemeral) {
            // Ghost/red-team: wipe the session so profiles never share state.
            cm.removeAllCookies(null);
            cm.flush();
            WebStorage.getInstance().deleteAllData();
            mContentWebView.clearCache(true);
            mContentWebView.clearHistory();
        }
        cm.setAcceptThirdPartyCookies(mContentWebView, acceptThirdParty);
        applyUserAgent(mContentWebView);  // re-evaluate UA for the new persona

        injectDashboardState();
        mContentWebView.loadUrl("file:///android_asset/start.html");
        Toast.makeText(this, "Profile: " + mPersona.displayName
                + (ephemeral ? " (ephemeral)" : ""), Toast.LENGTH_SHORT).show();
    }

    // ── Asset / native helpers ──────────────────────────────────────
    private String loadAsset(String path) {
        try (BufferedReader r = new BufferedReader(
                new InputStreamReader(getAssets().open(path)))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = r.readLine()) != null) sb.append(line).append('\n');
            return sb.toString();
        } catch (Exception e) {
            return "";
        }
    }

    /** Recursively copies an assets subtree to a real filesystem dir (idempotent). */
    private void extractAsset(String assetPath, java.io.File dest) throws Exception {
        String[] children = getAssets().list(assetPath);
        if (children != null && children.length > 0) {
            if (!dest.exists()) dest.mkdirs();
            for (String child : children) {
                extractAsset(assetPath + "/" + child, new java.io.File(dest, child));
            }
        } else {
            // Leaf file.
            java.io.File parent = dest.getParentFile();
            if (parent != null && !parent.exists()) parent.mkdirs();
            try (java.io.InputStream in = getAssets().open(assetPath);
                 java.io.FileOutputStream out = new java.io.FileOutputStream(dest)) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
        }
    }

    private interface NativeCall { String run(); }
    private String safeNative(NativeCall c) {
        try { return c.run(); } catch (Throwable t) { return null; }
    }
    private String nativeListPersonasSafe() { return nativeListPersonas(); }

    // ── Native core (shared C++ engine via JNI) ─────────────────────
    public native void initNativeBackend();
    public native boolean nativeInit(String runtimeRoot);
    public native String nativeListPersonas();
    public native String nativePersonaState(String personaId);
    public native String nativeFingerprintScript(String personaId);
    public native String nativeExtensionEvalBlock(String personaId);
    public native String nativeSecurityFlags(String personaId);
}
