// =============================================================================
// AndroidHttpClient - tiny Java helper for tcxCurl's Android backend
// =============================================================================
// Driving java.net.HttpURLConnection from raw JNI is doable but extremely
// verbose (every InputStream.read loop and every byte[] / String conversion
// is half a dozen JNI calls). Doing the request body assembly here keeps the
// C++ side to a single JNI hop per request: pass method/url/headers/body in,
// get a Result back out.
//
// No threading concerns inside this class: the C++ caller is already on a
// worker thread (HttpURLConnection forbids the Android main thread). We use
// HttpURLConnection (not HttpsURLConnection explicitly) — it auto-upgrades
// for https:// URLs.
//
// Lives under com.trussc.tcx.curl so the JNI lookup is stable regardless of
// the consuming app's package name.
// =============================================================================
package com.trussc.tcx.curl;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public final class AndroidHttpClient {

    /**
     * One-shot HTTP request result. C++ reads these fields directly via JNI
     * (GetFieldID), so the names/types are part of the ABI — don't rename
     * without updating tcxCurl_android.cpp.
     */
    public static final class Result {
        public int    statusCode;   // 0 on transport error
        public byte[] body;         // never null; empty on error
        public String error;        // null on success
    }

    /**
     * @param method        "GET", "POST", "PUT", "DELETE"
     * @param url           full URL (baseUrl + path joined on the C++ side)
     * @param headerNames   parallel array with headerValues; may be null
     * @param headerValues  parallel array with headerNames; may be null
     * @param body          request body bytes; may be null for GET/DELETE
     * @param connectTimeoutMs  socket connect timeout
     * @param readTimeoutMs     read timeout once connected
     */
    public static Result request(String method,
                                 String url,
                                 String[] headerNames,
                                 String[] headerValues,
                                 byte[]   body,
                                 int      connectTimeoutMs,
                                 int      readTimeoutMs) {
        Result r = new Result();
        r.body  = new byte[0];

        HttpURLConnection conn = null;
        try {
            URL u = new URL(url);
            conn = (HttpURLConnection) u.openConnection();
            conn.setRequestMethod(method);
            conn.setConnectTimeout(connectTimeoutMs);
            conn.setReadTimeout(readTimeoutMs);
            conn.setInstanceFollowRedirects(true);
            // Disable transparent gzip: HttpURLConnection auto-decompresses
            // unless we send our own Accept-Encoding, but then getContentLength
            // returns the compressed size which is misleading. Easier: opt out
            // entirely so the caller sees the response as-shipped.
            conn.setRequestProperty("Accept-Encoding", "identity");

            if (headerNames != null && headerValues != null) {
                int n = Math.min(headerNames.length, headerValues.length);
                for (int i = 0; i < n; i++) {
                    if (headerNames[i] == null) continue;
                    conn.setRequestProperty(headerNames[i], headerValues[i]);
                }
            }

            if (body != null && body.length > 0) {
                conn.setDoOutput(true);
                conn.setFixedLengthStreamingMode(body.length);
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(body);
                }
            }

            r.statusCode = conn.getResponseCode();

            InputStream is;
            if (r.statusCode >= 400) {
                is = conn.getErrorStream();
                if (is == null) is = conn.getInputStream();
            } else {
                is = conn.getInputStream();
            }

            if (is != null) {
                ByteArrayOutputStream buf = new ByteArrayOutputStream();
                byte[] chunk = new byte[8192];
                int read;
                while ((read = is.read(chunk)) != -1) {
                    buf.write(chunk, 0, read);
                }
                is.close();
                r.body = buf.toByteArray();
            }
        } catch (IOException | RuntimeException e) {
            r.error = e.getClass().getSimpleName() + ": " + e.getMessage();
        } finally {
            if (conn != null) conn.disconnect();
        }
        return r;
    }
}
