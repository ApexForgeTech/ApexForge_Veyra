package com.apexforge.veyra;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** App-owned settings only. This does not encrypt WebView's internal databases. */
final class SecurePreferences {
    private static final byte[] MAGIC = "VEYRA-ANDROID-V2\n".getBytes(StandardCharsets.US_ASCII);
    private final AtomicFile file;
    private final SecretKey inner;
    private final SecretKey outer;
    private JSONObject values;

    SecurePreferences(Context context) throws Exception {
        file = new AtomicFile(new File(context.getNoBackupFilesDir(), "preferences.vlt"));
        boolean existing = file.getBaseFile().exists()
                || new File(file.getBaseFile().getPath() + ".bak").exists();
        inner = key("com.apexforge.veyra.preferences.v2.inner", !existing);
        outer = key("com.apexforge.veyra.preferences.v2.outer", !existing);
        values = existing ? decode(file.readFully()) : new JSONObject();
        SharedPreferences legacy = context.getSharedPreferences("veyra_prefs", Context.MODE_PRIVATE);
        if (!existing) {
            for (Map.Entry<String, ?> entry : legacy.getAll().entrySet()) {
                if (!(entry.getValue() instanceof String))
                    throw new IllegalStateException("Unsupported legacy preference type");
                values.put(entry.getKey(), entry.getValue());
            }
            persist(values);
        }
        // Authenticate and parse the committed destination before removing the source.
        decode(file.readFully());
        if (!legacy.edit().clear().commit()) throw new IllegalStateException("Legacy cleanup failed");
        if (!context.deleteSharedPreferences("veyra_prefs"))
            throw new IllegalStateException("Legacy preference file cleanup failed");
    }

    private static SecretKey key(String alias, boolean create) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (!store.containsAlias(alias)) {
            if (!create) throw new IllegalStateException("Storage key is missing");
            KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
            generator.init(new KeyGenParameterSpec.Builder(alias,
                    KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256).setRandomizedEncryptionRequired(true).build());
            generator.generateKey();
        }
        return (SecretKey) store.getKey(alias, null);
    }

    private byte[] crypt(boolean encrypt, SecretKey key, byte[] input, String layer) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        if (encrypt) cipher.init(Cipher.ENCRYPT_MODE, key); // Keystore generates a fresh IV.
        else {
            if (input.length < 28) throw new IllegalStateException("Truncated encrypted settings");
            cipher.init(Cipher.DECRYPT_MODE, key, new GCMParameterSpec(128, input, 0, 12));
        }
        cipher.updateAAD(("VEYRA-ANDROID-V2:preferences:" + layer).getBytes(StandardCharsets.UTF_8));
        if (!encrypt) return cipher.doFinal(input, 12, input.length - 12);
        byte[] body = cipher.doFinal(input);
        byte[] iv = cipher.getIV();
        if (iv.length != 12) throw new IllegalStateException("Unexpected GCM IV length");
        byte[] result = Arrays.copyOf(iv, iv.length + body.length);
        System.arraycopy(body, 0, result, iv.length, body.length);
        return result;
    }

    private JSONObject decode(byte[] envelope) throws Exception {
        if (envelope.length > 8 * 1024 * 1024 || envelope.length < MAGIC.length + 56
                || !Arrays.equals(MAGIC, Arrays.copyOf(envelope, MAGIC.length)))
            throw new IllegalStateException("Invalid encrypted settings");
        byte[] wrapped = crypt(false, outer, Arrays.copyOfRange(envelope, MAGIC.length, envelope.length), "outer");
        byte[] plain = crypt(false, inner, wrapped, "inner");
        try { return new JSONObject(new String(plain, StandardCharsets.UTF_8)); }
        finally { Arrays.fill(plain, (byte) 0); Arrays.fill(wrapped, (byte) 0); }
    }

    private void persist(JSONObject next) throws Exception {
        byte[] plain = next.toString().getBytes(StandardCharsets.UTF_8);
        if (plain.length > 8 * 1024 * 1024 - 128) throw new IllegalStateException("Settings too large");
        byte[] wrapped;
        try { wrapped = crypt(true, outer, crypt(true, inner, plain, "inner"), "outer"); }
        finally { Arrays.fill(plain, (byte) 0); }
        FileOutputStream stream = null;
        try {
            stream = file.startWrite();
            stream.write(MAGIC);
            stream.write(wrapped);
            file.finishWrite(stream);
        } catch (Exception failure) {
            if (stream != null) file.failWrite(stream);
            throw failure;
        }
        JSONObject verified = decode(file.readFully());
        if (!verified.toString().equals(next.toString())) throw new IllegalStateException("Settings verification failed");
    }

    synchronized String getString(String key, String fallback) { return values.optString(key, fallback); }
    Editor edit() { return new Editor(); }
    final class Editor {
        private final Map<String, String> pending = new HashMap<>();
        Editor putString(String key, String value) { pending.put(key, value); return this; }
        void apply() {
            synchronized (SecurePreferences.this) {
                try {
                    JSONObject next = new JSONObject(values.toString());
                    for (Map.Entry<String, String> entry : pending.entrySet()) next.put(entry.getKey(), entry.getValue());
                    persist(next);
                    values = next;
                } catch (Exception e) {
                    // No plaintext fallback, no success return after failed persistence.
                    throw new IllegalStateException("Encrypted settings could not be saved", e);
                }
            }
        }
    }
}
