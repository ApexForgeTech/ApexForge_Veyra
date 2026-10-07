package com.apexforge.veyra;

import android.content.Context;
import android.util.Log;

import androidx.webkit.ProxyConfig;
import androidx.webkit.ProxyController;
import androidx.webkit.WebViewFeature;

import java.io.File;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.concurrent.Executors;

/**
 * Bundled Tor for Android — the Orbot / Tor-Browser model: ships the official
 * tor binary (jniLibs/<abi>/libTor.so, extracted to nativeLibraryDir) and runs
 * it inside the app, so anonymity works with NO system install. Tor exposes an
 * HTTP CONNECT tunnel that the WebView is routed through via androidx.webkit's
 * ProxyController (WebView can't use SOCKS directly, but HTTPTunnelPort works).
 */
public final class TorManager {

    private static final String TAG = "VeyraTor";
    public static final int SOCKS_PORT = 9050;
    public static final int HTTP_TUNNEL_PORT = 8118;   // WebView routes through this

    public interface StatusListener { void onTorStatus(String status, boolean ready); }

    private final Context app;
    private Process torProcess;
    private volatile boolean ready = false;

    public TorManager(Context context) { this.app = context.getApplicationContext(); }

    public boolean isReady() { return ready; }

    /** Launches the bundled tor if not already running, then routes the WebView
     *  through it once the HTTP tunnel is up. */
    public synchronized void start(final StatusListener listener) {
        if (torProcess != null && torProcess.isAlive()) {
            if (ready) applyWebViewProxy();
            notify(listener, "Tor already running", ready);
            return;
        }
        final File torBin = new File(app.getApplicationInfo().nativeLibraryDir, "libTor.so");
        if (!torBin.exists()) {
            notify(listener, "Bundled Tor binary missing for this ABI", false);
            return;
        }
        final File dataDir = new File(app.getFilesDir(), "tor-data");
        if (!dataDir.exists()) dataDir.mkdirs();

        try {
            ProcessBuilder pb = new ProcessBuilder(
                    torBin.getAbsolutePath(),
                    "--SocksPort", "127.0.0.1:" + SOCKS_PORT,
                    "--HTTPTunnelPort", "127.0.0.1:" + HTTP_TUNNEL_PORT,
                    "--DataDirectory", dataDir.getAbsolutePath(),
                    "--ControlPort", "0",
                    "--AvoidDiskWrites", "1",
                    "--ClientOnly", "1");
            pb.redirectErrorStream(true);
            torProcess = pb.start();
            notify(listener, "Tor starting…", false);
        } catch (Exception e) {
            Log.e(TAG, "Failed to launch Tor", e);
            notify(listener, "Failed to launch Tor: " + e.getMessage(), false);
            return;
        }

        // Wait (off the UI thread) for the HTTP tunnel, then route the WebView.
        Executors.newSingleThreadExecutor().execute(() -> {
            boolean up = false;
            for (int i = 0; i < 120 && torProcess.isAlive(); i++) {  // up to ~60s
                if (portOpen(HTTP_TUNNEL_PORT)) { up = true; break; }
                try { Thread.sleep(500); } catch (InterruptedException ignored) {}
            }
            ready = up;
            final boolean fUp = up;
            // Proxy override must be set on a thread with a Looper → post to main.
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                if (fUp) applyWebViewProxy();
                notify(listener, fUp ? "Tor connected — routing through Tor"
                                     : "Tor failed to bootstrap", fUp);
            });
        });
    }

    public synchronized void stop() {
        ready = false;
        clearWebViewProxy();
        if (torProcess != null) { torProcess.destroy(); torProcess = null; }
    }

    /** Routes ALL WebView traffic through Tor's local HTTP tunnel. */
    public void applyWebViewProxy() {
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.PROXY_OVERRIDE)) {
            Log.w(TAG, "WebView PROXY_OVERRIDE unsupported on this system WebView");
            return;
        }
        ProxyConfig cfg = new ProxyConfig.Builder()
                .addProxyRule("127.0.0.1:" + HTTP_TUNNEL_PORT)
                .addDirect()  // fallback for localhost/loopback
                .bypassSimpleHostnames()
                .build();
        ProxyController.getInstance().setProxyOverride(cfg,
                Executors.newSingleThreadExecutor(), () -> Log.i(TAG, "WebView routed via Tor"));
    }

    public void clearWebViewProxy() {
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.PROXY_OVERRIDE)) return;
        ProxyController.getInstance().clearProxyOverride(
                Executors.newSingleThreadExecutor(), () -> Log.i(TAG, "WebView proxy cleared"));
    }

    private static boolean portOpen(int port) {
        try (Socket s = new Socket()) {
            s.connect(new InetSocketAddress("127.0.0.1", port), 400);
            return true;
        } catch (Exception e) { return false; }
    }

    private void notify(StatusListener l, String msg, boolean rdy) {
        Log.i(TAG, msg);
        if (l != null) l.onTorStatus(msg, rdy);
    }
}
