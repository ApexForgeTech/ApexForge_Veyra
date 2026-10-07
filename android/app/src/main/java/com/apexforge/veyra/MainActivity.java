package com.apexforge.veyra;

import android.Manifest;
import android.app.AlertDialog;
import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import androidx.core.content.ContextCompat;
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
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URLEncoder;
import java.util.HashMap;
import java.util.Map;

public class MainActivity extends AppCompatActivity {

    private WebView mUiWebView;
    private WebView mContentWebView;
    private FrameLayout mContentContainer;
    private FrameLayout mSidebarContainer;
    private EditText mAddressBar;
    private ProgressBar mPageProgress;
    private Button mBackButton;
    private Button mForwardButton;
    private boolean isPanelOpen = false;

    private static final String PREFS = "veyra_prefs";
    private static final String CUSTOM_PERSONAS_PREF = "custom_personas";
    private SecurePreferences mPrefs;
    private ValueCallback<Uri[]> mFileCallback;
    private ActivityResultLauncher<Intent> mFileChooser;
    // A web page's camera/mic request, parked while we obtain the matching OS
    // runtime permission (granting the WebView alone is not enough on Android 6+).
    private PermissionRequest mPendingWebPermission;
    private String[] mPendingWebResources;
    private ActivityResultLauncher<String[]> mRuntimePerms;

    // Security profile + extension content script (parity with desktop).
    private VeyraProfiles.Persona mPersona;       // fallback when native core is unavailable
    private String mControlScript = "";           // extensions/browser-control/control.js
    private boolean mNativeReady = false;         // C++ core (schemas) loaded?
    private String mActivePersonaId = "real_identity";
    private TorManager mTor;                       // bundled Tor (jniLibs/libTor.so)
    private I2pManager mI2p;                        // bundled I2P (jniLibs/libi2pd.so)
    private final Map<String, WebView> mPersistentProfileViews = new HashMap<>();
    private WebView mEphemeralProfileView;
    private String mEphemeralProfileId;

    static {
        System.loadLibrary("veyra_native");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        VeyraApplication app = (VeyraApplication) getApplication();
        if (!app.storageReady) {
            new AlertDialog.Builder(this).setTitle("Secure storage unavailable")
                    .setMessage("Veyra could not unlock or upgrade its protected data. Browsing has been stopped. Unlock the device and restart; do not clear app data if you need the saved settings.")
                    .setPositiveButton("Close", (dialog, which) -> finish())
                    .setCancelable(false).show();
            return;
        }
        mPrefs = app.preferences;
        Window window = getWindow();
        window.setStatusBarColor(0xFF0D121C);
        window.setNavigationBarColor(0xFF0D121C);
        setContentView(R.layout.activity_main);

        mActivePersonaId = mPrefs.getString("persona", "real_identity");
        mPersona = resolvePersona(mActivePersonaId);
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
                    if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                        android.content.Intent data = result.getData();
                        if (data.getClipData() != null) {            // multiple files
                            int n = data.getClipData().getItemCount();
                            uris = new Uri[n];
                            for (int i = 0; i < n; i++) uris[i] = data.getClipData().getItemAt(i).getUri();
                        } else if (data.getData() != null) {          // single file
                            uris = new Uri[]{data.getData()};
                        }
                    }
                    mFileCallback.onReceiveValue(uris);
                    mFileCallback = null;
                });

        // OS runtime permissions (camera/mic) for a parked web request.
        mRuntimePerms = registerForActivityResult(
                new ActivityResultContracts.RequestMultiplePermissions(), result -> {
                    if (mPendingWebPermission == null) return;
                    boolean allGranted = !result.isEmpty();
                    for (Boolean granted : result.values()) {
                        if (granted == null || !granted) allGranted = false;
                    }
                    if (allGranted) {
                        mPendingWebPermission.grant(mPendingWebResources);
                    } else {
                        mPendingWebPermission.deny();
                        Toast.makeText(this, "System denied the permission", Toast.LENGTH_SHORT).show();
                    }
                    mPendingWebPermission = null;
                    mPendingWebResources = null;
                });

        mSidebarContainer = findViewById(R.id.sidebar_container);
        mAddressBar = findViewById(R.id.address_bar);
        mPageProgress = findViewById(R.id.page_progress);
        mBackButton = findViewById(R.id.btn_back);
        mForwardButton = findViewById(R.id.btn_forward);
        Button reloadButton = findViewById(R.id.btn_reload);
        Button menuButton = findViewById(R.id.btn_menu);
        Button closePanelButton = findViewById(R.id.btn_close_panel);
        FloatingActionButton toggleBtn = findViewById(R.id.fab_toggle_panel);
        menuButton.setOnClickListener(v -> showBrowserMenu(v));
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

        // 2. Setup Main Browser. Persistent personas keep their own WebView
        // instance so navigation history and in-memory page state survive
        // switching; ephemeral personas get a throwaway instance.
        mContentContainer = findViewById(R.id.content_webview_container);
        switchContentWebView(mActivePersonaId, mPersona != null && mPersona.ephemeral, null);
        setupBrowserChrome();
        String launchUrl = getIntentUrl(getIntent());
        mContentWebView.loadUrl(launchUrl == null ? "file:///android_asset/start.html" : launchUrl);

        // If the persisted persona uses Tor, bring up the bundled Tor now.
        if (mPersona != null && ("tor".equals(mPersona.routeType) || "chained".equals(mPersona.routeType))) {
            applyRouting(mPersona.routeType);
        }

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
        settings.setSaveFormData(false);
        settings.setSavePassword(false);
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
        String state = null;
        if (hasCustomPersonas()) {
            state = VeyraProfiles.buildStateJson(mActivePersonaId, "1.0-mobile", loadCustomPersonas());
        } else if (mNativeReady) {
            state = safeNative(() -> nativePersonaState(mActivePersonaId));
        }
        if (state == null || state.length() < 3) {
            state = VeyraProfiles.buildStateJson(mActivePersonaId, "1.0-mobile", loadCustomPersonas());
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

    private WebView createContentWebView() {
        WebView web = new WebView(this);
        web.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        web.setBackgroundColor(0xFF0D121C);
        setupWebView(web);
        enhanceContentWebView(web);
        web.addJavascriptInterface(new VeyraBridge(), "Veyra");
        return web;
    }

    private void switchContentWebView(String personaId, boolean ephemeral, String firstUrl) {
        if (mContentContainer == null) return;

        WebView oldEphemeralToDestroy = null;
        if (mEphemeralProfileView != null
                && (mEphemeralProfileId == null || !mEphemeralProfileId.equals(personaId))) {
            oldEphemeralToDestroy = mEphemeralProfileView;
        }

        WebView next;
        if (ephemeral) {
            mEphemeralProfileId = personaId;
            mEphemeralProfileView = createContentWebView();
            next = mEphemeralProfileView;
        } else {
            next = mPersistentProfileViews.get(personaId);
            if (next == null) {
                next = createContentWebView();
                mPersistentProfileViews.put(personaId, next);
            }
        }

        if (mContentWebView != null && mContentWebView.getParent() == mContentContainer) {
            mContentContainer.removeView(mContentWebView);
        }
        mContentWebView = next;
        if (next.getParent() instanceof FrameLayout) {
            ((FrameLayout) next.getParent()).removeView(next);
        }
        mContentContainer.addView(next);

        CookieManager.getInstance().setAcceptThirdPartyCookies(next, true);
        applyUserAgent(next);
        updateAddressBar(next.getUrl());
        updateNavigationButtons();

        if (firstUrl != null && next.getUrl() == null) {
            next.loadUrl(firstUrl);
        }

        if (oldEphemeralToDestroy != null && oldEphemeralToDestroy != next) {
            destroyDetachedWebView(oldEphemeralToDestroy);
            if (oldEphemeralToDestroy == mEphemeralProfileView) {
                mEphemeralProfileView = null;
                mEphemeralProfileId = null;
            }
        }
    }

    private void destroyDetachedWebView(WebView web) {
        if (web == null || web == mContentWebView) return;
        if (web.getParent() instanceof FrameLayout) {
            ((FrameLayout) web.getParent()).removeView(web);
        }
        web.stopLoading();
        web.clearHistory();
        web.clearFormData();
        web.destroy();
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

    private VeyraProfiles.Persona resolvePersona(String id) {
        return VeyraProfiles.byId(id, loadCustomPersonas());
    }

    private boolean isCustomPersona(String id) {
        for (VeyraProfiles.Persona p : loadCustomPersonas()) {
            if (p.id.equals(id)) return true;
        }
        return false;
    }

    private boolean hasCustomPersonas() {
        return loadCustomPersonas().length > 0;
    }

    private VeyraProfiles.Persona[] loadCustomPersonas() {
        if (mPrefs == null) return new VeyraProfiles.Persona[0];
        try {
            JSONArray arr = new JSONArray(mPrefs.getString(CUSTOM_PERSONAS_PREF, "[]"));
            VeyraProfiles.Persona[] out = new VeyraProfiles.Persona[arr.length()];
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                out[i] = new VeyraProfiles.Persona(
                        o.optString("id"),
                        o.optString("display_name"),
                        o.optString("security_mode", "hardened"),
                        o.optString("route_profile_id", "direct_isp"),
                        o.optString("route_type", "direct"),
                        o.optString("fingerprint_profile_id", "balanced_stealth"),
                        o.optString("extension_policy_id", "minimal_locked"),
                        o.optBoolean("ephemeral", false));
            }
            return out;
        } catch (Exception e) {
            return new VeyraProfiles.Persona[0];
        }
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
                // Release any previous pending callback so the input never hangs.
                if (mFileCallback != null) { mFileCallback.onReceiveValue(null); }
                mFileCallback = callback;
                Intent intent;
                try {
                    intent = params.createIntent();
                } catch (Exception e) {
                    intent = null;
                }
                if (intent == null) {
                    // Robust fallback that works on every Android version.
                    intent = new Intent(Intent.ACTION_GET_CONTENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                }
                // Honor accept= types + multiple when the page asked for them.
                try {
                    String[] accept = params.getAcceptTypes();
                    if (accept != null && accept.length > 0 && accept[0] != null && !accept[0].isEmpty()) {
                        intent.setType(accept.length == 1 ? accept[0] : "*/*");
                        if (accept.length > 1) intent.putExtra(Intent.EXTRA_MIME_TYPES, accept);
                    }
                    if (params.getMode() == FileChooserParams.MODE_OPEN_MULTIPLE) {
                        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                    }
                } catch (Exception ignored) {}
                try {
                    mFileChooser.launch(intent);
                    return true;
                } catch (Exception e) {
                    if (mFileCallback != null) { mFileCallback.onReceiveValue(null); mFileCallback = null; }
                    Toast.makeText(MainActivity.this, "No file picker available", Toast.LENGTH_SHORT).show();
                    return false;
                }
            }
            @Override
            public void onPermissionRequest(final PermissionRequest request) {
                final String origin = request.getOrigin() != null
                        ? request.getOrigin().toString() : "";
                // Map the requested resources to our stored per-site decision.
                String permName = "Camera";
                for (String r : request.getResources()) {
                    if (r.contains("AUDIO")) permName = "Microphone";
                    else if (r.contains("VIDEO")) permName = "Camera";
                }
                final String stored = mPrefs.getString("perm:" + trimOrigin(origin) + ":" + permName, "ask");
                if ("allow".equals(stored)) { grantWebPermission(request); return; }
                if ("block".equals(stored)) { request.deny(); return; }
                final String fName = permName;
                runOnUiThread(() -> new AlertDialog.Builder(MainActivity.this)
                        .setTitle(fName + " access")
                        .setMessage(trimOrigin(origin) + " wants to use your " + fName.toLowerCase())
                        .setPositiveButton("Allow", (d, w) -> {
                            mPrefs.edit().putString("perm:" + trimOrigin(origin) + ":" + fName, "allow").apply();
                            grantWebPermission(request);
                        })
                        .setNegativeButton("Block", (d, w) -> {
                            mPrefs.edit().putString("perm:" + trimOrigin(origin) + ":" + fName, "block").apply();
                            request.deny();
                        })
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
        final String[] items = {"Site info", "Find in page", "Downloads",
                                "Profiles / security panel", "Desktop site", "Share", "Settings"};
        new AlertDialog.Builder(this)
                .setItems(items, (d, which) -> {
                    switch (which) {
                        case 0: showSiteInfo(); break;
                        case 1: showFindDialog(); break;
                        case 2:
                            try { startActivity(new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS)); }
                            catch (Exception ignored) {}
                            break;
                        case 3: if (isPanelOpen) closePanel(); else openPanel(); break;
                        case 4: toggleDesktopSite(); break;
                        case 5: shareCurrent(); break;
                        case 6: showSettings(); break;
                    }
                })
                .show();
    }

    // Chrome-style site info: origin, TLS, and per-site permission toggles.
    private void showSiteInfo() {
        String url = mContentWebView != null ? mContentWebView.getUrl() : null;
        if (url == null) url = "";
        final String origin = url.contains("/") && url.indexOf("://") > 0
                ? url.substring(0, url.indexOf('/', url.indexOf("://") + 3) < 0
                        ? url.length() : url.indexOf('/', url.indexOf("://") + 3))
                : url;
        boolean secure = url.startsWith("https://");
        final String[] perms = {"Camera", "Microphone", "Location", "Notifications"};
        new AlertDialog.Builder(this)
                .setTitle(origin.isEmpty() ? "Site information" : origin)
                .setMessage((secure ? "🔒 Secure (HTTPS)\n" : "⚠ Not secure (HTTP)\n")
                        + "Route: " + (mTor != null && mTor.isReady() ? "Tor" : mPersona.routeType)
                        + "\n\nManage permissions for this site:")
                .setItems(perms, (d, which) -> showPermissionToggle(origin, perms[which]))
                .setPositiveButton("Close", null)
                .show();
    }

    /** Grants a web camera/mic request, first obtaining the matching OS runtime
     *  permission when the app doesn't already hold it. Granting the WebView
     *  request alone does nothing if the app itself lacks CAMERA/RECORD_AUDIO. */
    private void grantWebPermission(PermissionRequest request) {
        java.util.List<String> needed = new java.util.ArrayList<>();
        for (String r : request.getResources()) {
            if (PermissionRequest.RESOURCE_VIDEO_CAPTURE.equals(r)
                    && ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                            != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.CAMERA);
            }
            if (PermissionRequest.RESOURCE_AUDIO_CAPTURE.equals(r)
                    && ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                            != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.RECORD_AUDIO);
            }
        }
        if (needed.isEmpty()) {
            request.grant(request.getResources());
            return;
        }
        // Park the web request and ask the OS; the launcher callback completes it.
        mPendingWebPermission = request;
        mPendingWebResources = request.getResources();
        mRuntimePerms.launch(needed.toArray(new String[0]));
    }

    // Normalizes an origin string (drops a trailing slash) for permission keys.
    private static String trimOrigin(String o) {
        if (o == null) return "";
        return o.endsWith("/") ? o.substring(0, o.length() - 1) : o;
    }

    private void showPermissionToggle(String origin, String perm) {
        new AlertDialog.Builder(this)
                .setTitle(perm + " — " + origin)
                .setItems(new String[]{"Allow", "Block", "Ask (default)"}, (d, which) -> {
                    String decision = which == 0 ? "allow" : which == 1 ? "block" : "ask";
                    mPrefs.edit().putString("perm:" + origin + ":" + perm, decision).apply();
                    Toast.makeText(this, perm + ": " + decision, Toast.LENGTH_SHORT).show();
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
        if (mContentWebView != null) CookieManager.getInstance().flush();
    }

    @Override
    protected void onDestroy() {
        if (mContentWebView != null) CookieManager.getInstance().flush();
        for (WebView web : mPersistentProfileViews.values()) {
            if (web != mContentWebView) destroyDetachedWebView(web);
        }
        mPersistentProfileViews.clear();
        if (mEphemeralProfileView != mContentWebView) {
            destroyDetachedWebView(mEphemeralProfileView);
        }
        if (mTor != null) mTor.stop();
        if (mI2p != null) mI2p.stop();
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
                case "switch_persona": {
                    String personaId = o.optString("persona_id", "");
                    if (!personaId.isEmpty()) applyPersona(personaId);
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
            JSONArray arr = new JSONArray(
                    mNativeReady && !hasCustomPersonas() ? safeNative(this::nativeListPersonasSafe) : "[]");
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
        VeyraProfiles.Persona[] custom = loadCustomPersonas();
        String[] mergedIds = new String[ids.length + custom.length + 1];
        String[] mergedLabels = new String[labels.length + custom.length + 1];
        System.arraycopy(ids, 0, mergedIds, 0, ids.length);
        System.arraycopy(labels, 0, mergedLabels, 0, labels.length);
        for (int i = 0; i < custom.length; i++) {
            VeyraProfiles.Persona p = custom[i];
            int idx = ids.length + i;
            mergedIds[idx] = p.id;
            mergedLabels[idx] = p.displayName + "  ·  " + p.securityMode
                    + (p.ephemeral ? "  (ephemeral)" : "  (custom)");
        }
        mergedIds[mergedIds.length - 1] = "__create_custom__";
        mergedLabels[mergedLabels.length - 1] = "Create custom profile...";
        final String[] fIds = mergedIds;
        new AlertDialog.Builder(this)
                .setTitle("Security profile")
                .setItems(mergedLabels, (d, which) -> {
                    if ("__create_custom__".equals(fIds[which])) {
                        showCreateCustomPersonaDialog();
                    } else {
                        applyPersona(fIds[which]);
                    }
                    d.dismiss();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void showCreateCustomPersonaDialog() {
        LinearLayout form = new LinearLayout(this);
        form.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (getResources().getDisplayMetrics().density * 18);
        form.setPadding(pad, pad / 2, pad, 0);

        EditText name = new EditText(this);
        name.setHint("Profile name");
        form.addView(name);

        Spinner mode = new Spinner(this);
        String[] modes = {"hardened", "casual", "ghost", "redteam", "airgap"};
        mode.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, modes));
        form.addView(mode);

        Spinner route = new Spinner(this);
        String[] routes = {"Direct ISP", "VPN Tunnel", "Tor Bridge", "I2P Network", "Chained Operations"};
        route.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, routes));
        form.addView(route);

        CheckBox ephemeral = new CheckBox(this);
        ephemeral.setText("Ephemeral: destroy this profile session on exit");
        form.addView(ephemeral);

        new AlertDialog.Builder(this)
                .setTitle("Create custom profile")
                .setView(form)
                .setPositiveButton("Create", (d, w) -> createCustomPersona(
                        name.getText().toString(), modes[mode.getSelectedItemPosition()],
                        route.getSelectedItemPosition(), ephemeral.isChecked()))
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void createCustomPersona(String rawName, String mode, int routeIndex, boolean ephemeral) {
        String displayName = rawName == null ? "" : rawName.trim();
        if (displayName.isEmpty()) displayName = "Custom Profile";
        String idBase = displayName.toLowerCase().replaceAll("[^a-z0-9]+", "_")
                .replaceAll("^_+|_+$", "");
        if (idBase.isEmpty()) idBase = "custom";
        String id = idBase;
        int suffix = 2;
        while (resolvePersona(id).id.equals(id) || isCustomPersona(id)) {
            id = idBase + "_" + suffix++;
            if (suffix > 99) break;
        }

        String routeProfile = "direct_isp";
        String routeType = "direct";
        if (routeIndex == 1) { routeProfile = "vpn_tunnel"; routeType = "vpn"; }
        else if (routeIndex == 2) { routeProfile = "tor_bridge"; routeType = "tor"; }
        else if (routeIndex == 3) { routeProfile = "i2p_network"; routeType = "i2p"; }
        else if (routeIndex == 4) { routeProfile = "chained_ops"; routeType = "chained"; }

        String fingerprint = "casual".equals(mode) ? "native_stable"
                : ("redteam".equals(mode) || "airgap".equals(mode) ? "max_entropy" : "balanced_stealth");
        String extensionPolicy = "casual".equals(mode) ? "trusted_daily"
                : ("redteam".equals(mode) ? "operator_tools" : "minimal_locked");

        try {
            JSONArray arr = new JSONArray(mPrefs.getString(CUSTOM_PERSONAS_PREF, "[]"));
            JSONObject o = new JSONObject();
            o.put("id", id);
            o.put("display_name", displayName);
            o.put("security_mode", mode);
            o.put("route_profile_id", routeProfile);
            o.put("route_type", routeType);
            o.put("fingerprint_profile_id", fingerprint);
            o.put("extension_policy_id", extensionPolicy);
            o.put("ephemeral", ephemeral);
            arr.put(o);
            mPrefs.edit().putString(CUSTOM_PERSONAS_PREF, arr.toString()).apply();
            applyPersona(id);
        } catch (Exception e) {
            Toast.makeText(this, "Could not create profile", Toast.LENGTH_SHORT).show();
        }
    }

    /** Switches the active persona and applies its security model to the WebView. */
    private void applyPersona(String personaId) {
        if (personaId == null || personaId.equals(mActivePersonaId)) return;
        mActivePersonaId = personaId;
        mPersona = resolvePersona(personaId);
        mPrefs.edit().putString("persona", personaId).apply();

        // Apply engine-computed security flags (ephemeral storage, cookie policy).
        boolean ephemeral = mPersona.ephemeral;
        boolean acceptThirdParty = true;
        String routeType = mPersona.routeType;
        try {
            if (mNativeReady && !isCustomPersona(personaId)) {
                JSONObject flags = new JSONObject(safeNative(() -> nativeSecurityFlags(personaId)));
                ephemeral = flags.optBoolean("ephemeral", ephemeral);
                acceptThirdParty = flags.optBoolean("allow_third_party_frames", true);
                routeType = flags.optString("route_type", routeType);
            }
        } catch (Exception ignored) {}

        // Route through the bundled Tor for tor/chained personas; otherwise
        // clear any Tor routing. (I2P on mobile pending bundled i2pd.)
        applyRouting(routeType);

        CookieManager cm = CookieManager.getInstance();
        switchContentWebView(personaId, ephemeral, "file:///android_asset/start.html");
        cm.setAcceptThirdPartyCookies(mContentWebView, acceptThirdParty);
        applyUserAgent(mContentWebView);  // re-evaluate UA for the new persona

        injectDashboardState();
        Toast.makeText(this, "Profile: " + mPersona.displayName
                + (ephemeral ? " (ephemeral)" : ""), Toast.LENGTH_SHORT).show();
    }

    // Starts/stops the bundled Tor / I2P routers and routes the WebView for the
    // active route. Tor and I2P are both shipped in-app (jniLibs); only one runs
    // at a time and the other is torn down so their proxies never conflict.
    private void applyRouting(String routeType) {
        if (mTor == null) mTor = new TorManager(this);
        if (mI2p == null) mI2p = new I2pManager(this);
        final boolean useTor = "tor".equals(routeType) || "chained".equals(routeType);
        final boolean useI2p = "i2p".equals(routeType);
        if (useTor) {
            mI2p.stop();
            mTor.start((status, ready) -> runOnUiThread(() -> {
                Toast.makeText(this, "Tor: " + status, Toast.LENGTH_SHORT).show();
                if (ready && mContentWebView != null) mContentWebView.reload();
            }));
        } else if (useI2p) {
            mTor.stop();
            mI2p.start((status, ready) -> runOnUiThread(() -> {
                Toast.makeText(this, "I2P: " + status, Toast.LENGTH_SHORT).show();
                if (ready && mContentWebView != null) mContentWebView.reload();
            }));
        } else {
            mTor.stop();   // clears the WebView proxy too
            mI2p.stop();
        }
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
