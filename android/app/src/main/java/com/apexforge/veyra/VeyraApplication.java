package com.apexforge.veyra;

import android.app.Application;
import java.io.File;
import java.io.IOException;

/** Runs the upgrade before any WebView (including the layout's dashboard) is created. */
public final class VeyraApplication extends Application {
    SecurePreferences preferences;
    boolean storageReady;

    @Override public void onCreate() {
        super.onCreate();
        try {
            preferences = new SecurePreferences(this);
            if (!"2".equals(preferences.getString("storage_upgrade", ""))) {
                // Explicit upgrade policy: discard old cookies, DOM storage, caches,
                // HTTP credentials and form data, including SQLite journal/WAL files.
                File[] children = getDataDir().listFiles();
                if (children == null) throw new IOException("Cannot enumerate app data");
                for (File child : children) {
                    if (child.getName().equals("app_webview") || child.getName().startsWith("app_webview_"))
                        removeTree(child, getDataDir());
                }
                removeTree(new File(getCacheDir(), "WebView"), getCacheDir());
                // Pre-WebView legacy databases on older installations.
                deleteDatabase("webview.db");
                deleteDatabase("webviewCache.db");
                preferences.edit().putString("storage_upgrade", "2").apply();
            }
            storageReady = true;
        } catch (Exception failure) {
            // The Activity shows an error before inflating a WebView. Retry next launch.
            storageReady = false;
        }
    }

    private static void removeTree(File file, File root) throws IOException {
        String canonical = file.getCanonicalPath();
        if (!canonical.startsWith(root.getCanonicalPath() + File.separator)
                || !canonical.equals(file.getAbsolutePath()))
            throw new IOException("Unsafe legacy storage path");
        if (!file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot read legacy storage");
            for (File child : children) removeTree(child, root);
        }
        if (!file.delete()) throw new IOException("Cannot remove legacy browser storage");
    }
}
