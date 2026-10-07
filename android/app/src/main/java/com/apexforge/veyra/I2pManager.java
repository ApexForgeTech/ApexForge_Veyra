package com.apexforge.veyra;

import android.content.Context;
import android.util.Log;

import androidx.webkit.ProxyConfig;
import androidx.webkit.ProxyController;
import androidx.webkit.WebViewFeature;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.concurrent.Executors;

/**
 * Bundled I2P (i2pd) for Android — the same model as {@link TorManager}: ships
 * the official i2pd binary (jniLibs/<abi>/libi2pd.so, extracted to
 * nativeLibraryDir) and runs it inside the app, so I2P works with NO system
 * install. i2pd exposes a local HTTP proxy on 4444 that the WebView is routed
 * through via androidx.webkit's ProxyController. Reseed/family certificates ship
 * in assets/i2pd/certificates and are seeded into the data dir on first run so
 * the router can reseed (it looks for them under <datadir>/certificates).
 */
public final class I2pManager {

    private static final String TAG = "VeyraI2P";
    public static final int HTTP_PROXY_PORT = 4444;

    public interface StatusListener { void onStatus(String status, boolean ready); }

    private final Context app;
    private Process proc;
    private volatile boolean ready = false;

    public I2pManager(Context context) { this.app = context.getApplicationContext(); }

    public boolean isReady() { return ready; }

    /** Launches the bundled i2pd if not already running, then routes the WebView
     *  through its HTTP proxy once it is up. */
    public synchronized void start(final StatusListener listener) {
        if (proc != null && proc.isAlive()) {
            if (ready) applyWebViewProxy();
            notify(listener, "I2P already running", ready);
            return;
        }
        final File bin = new File(app.getApplicationInfo().nativeLibraryDir, "libi2pd.so");
        if (!bin.exists()) {
            notify(listener, "Bundled i2pd missing for this ABI", false);
            return;
        }
        final File dataDir = new File(app.getFilesDir(), "i2pd-data");
        if (!dataDir.exists()) dataDir.mkdirs();
        // Seed reseed/family certificates once (i2pd verifies reseed over HTTPS
        // against these and won't bootstrap without them).
        final File certs = new File(dataDir, "certificates");
        if (!certs.exists()) { certs.mkdirs(); copyAssetDir("i2pd/certificates", certs); }

        try {
            ProcessBuilder pb = new ProcessBuilder(
                    bin.getAbsolutePath(),
                    "--datadir=" + dataDir.getAbsolutePath(),
                    "--httpproxy.enabled=true",
                    "--httpproxy.address=127.0.0.1",
                    "--httpproxy.port=" + HTTP_PROXY_PORT,
                    "--socksproxy.enabled=false",
                    "--ipv6=false");
            pb.redirectErrorStream(true);
            proc = pb.start();
            notify(listener, "I2P starting… (reseed can take a minute)", false);
        } catch (Exception e) {
            Log.e(TAG, "Failed to launch i2pd", e);
            notify(listener, "Failed to launch i2pd: " + e.getMessage(), false);
            return;
        }

        Executors.newSingleThreadExecutor().execute(() -> {
            boolean up = false;
            for (int i = 0; i < 240 && proc.isAlive(); i++) {  // up to ~2 min (I2P reseed is slow)
                if (portOpen(HTTP_PROXY_PORT)) { up = true; break; }
                try { Thread.sleep(500); } catch (InterruptedException ignored) {}
            }
            ready = up;
            final boolean fUp = up;
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (fUp) applyWebViewProxy();
                notify(listener, fUp ? "I2P connected — routing through I2P"
                                     : "I2P still bootstrapping", fUp);
            });
        });
    }

    public synchronized void stop() {
        ready = false;
        clearWebViewProxy();
        if (proc != null) { proc.destroy(); proc = null; }
    }

    /** Routes ALL WebView traffic through i2pd's local HTTP proxy. */
    public void applyWebViewProxy() {
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.PROXY_OVERRIDE)) {
            Log.w(TAG, "WebView PROXY_OVERRIDE unsupported on this system WebView");
            return;
        }
        ProxyConfig cfg = new ProxyConfig.Builder()
                .addProxyRule("127.0.0.1:" + HTTP_PROXY_PORT)
                .addDirect()
                .bypassSimpleHostnames()
                .build();
        ProxyController.getInstance().setProxyOverride(cfg,
                Executors.newSingleThreadExecutor(), () -> Log.i(TAG, "WebView routed via I2P"));
    }

    public void clearWebViewProxy() {
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.PROXY_OVERRIDE)) return;
        ProxyController.getInstance().clearProxyOverride(
                Executors.newSingleThreadExecutor(), () -> Log.i(TAG, "WebView proxy cleared"));
    }

    /** Recursively copies an assets subtree into a real filesystem dir. */
    private void copyAssetDir(String assetPath, File dest) {
        try {
            String[] children = app.getAssets().list(assetPath);
            if (children != null && children.length > 0) {
                if (!dest.exists()) dest.mkdirs();
                for (String c : children) copyAssetDir(assetPath + "/" + c, new File(dest, c));
            } else {
                File parent = dest.getParentFile();
                if (parent != null && !parent.exists()) parent.mkdirs();
                try (InputStream in = app.getAssets().open(assetPath);
                     OutputStream out = new FileOutputStream(dest)) {
                    byte[] buf = new byte[8192];
                    int n;
                    while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "cert seed failed: " + assetPath, e);
        }
    }

    private static boolean portOpen(int port) {
        try (Socket s = new Socket()) {
            s.connect(new InetSocketAddress("127.0.0.1", port), 400);
            return true;
        } catch (Exception e) { return false; }
    }

    private void notify(StatusListener l, String msg, boolean rdy) {
        Log.i(TAG, msg);
        if (l != null) l.onStatus(msg, rdy);
    }
}
