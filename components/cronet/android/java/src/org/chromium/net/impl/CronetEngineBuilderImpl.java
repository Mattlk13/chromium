// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net.impl;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_DISABLED;
import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_DISK;
import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP;
import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_IN_MEMORY;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Constructor;
import java.net.IDN;
import java.util.Date;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Implementation of {@link ICronetEngineBuilder}.
 */
public class CronetEngineBuilderImpl extends ICronetEngineBuilder {
    /**
     * A hint that a host supports QUIC.
     */
    public static class QuicHint {
        // The host.
        final String mHost;
        // Port of the server that supports QUIC.
        final int mPort;
        // Alternate protocol port.
        final int mAlternatePort;

        QuicHint(String host, int port, int alternatePort) {
            mHost = host;
            mPort = port;
            mAlternatePort = alternatePort;
        }
    }

    /**
     * A public key pin.
     */
    public static class Pkp {
        // Host to pin for.
        final String mHost;
        // Array of SHA-256 hashes of keys.
        final byte[][] mHashes;
        // Should pin apply to subdomains?
        final boolean mIncludeSubdomains;
        // When the pin expires.
        final Date mExpirationDate;

        Pkp(String host, byte[][] hashes, boolean includeSubdomains, Date expirationDate) {
            mHost = host;
            mHashes = hashes;
            mIncludeSubdomains = includeSubdomains;
            mExpirationDate = expirationDate;
        }
    }

    private static final String TAG = "CronetEngineBuilder";
    private static final String NATIVE_CRONET_IMPL_CLASS =
            "org.chromium.net.impl.CronetUrlRequestContext";
    private static final String JAVA_CRONET_IMPL_CLASS = "org.chromium.net.impl.JavaCronetEngine";
    private static final Pattern INVALID_PKP_HOST_NAME = Pattern.compile("^[0-9\\.]*$");

    // Private fields are simply storage of configuration for the resulting CronetEngine.
    // See setters below for verbose descriptions.
    private final Context mApplicationContext;
    private final List<QuicHint> mQuicHints = new LinkedList<>();
    private final List<Pkp> mPkps = new LinkedList<>();
    private boolean mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
    private String mUserAgent;
    private String mStoragePath;
    private boolean mLegacyModeEnabled;
    private VersionSafeCallbacks.LibraryLoader mLibraryLoader;
    private boolean mQuicEnabled;
    private boolean mHttp2Enabled;
    private boolean mSdchEnabled;
    private String mDataReductionProxyKey;
    private String mDataReductionProxyPrimaryProxy;
    private String mDataReductionProxyFallbackProxy;
    private String mDataReductionProxySecureProxyCheckUrl;
    private boolean mDisableCache;
    private int mHttpCacheMode;
    private long mHttpCacheMaxSize;
    private String mExperimentalOptions;
    private long mMockCertVerifier;
    private boolean mNetworkQualityEstimatorEnabled;
    private String mCertVerifierData;

    /**
     * Default config enables SPDY, disables QUIC, SDCH and HTTP cache.
     * @param context Android {@link Context} for engine to use.
     */
    public CronetEngineBuilderImpl(Context context) {
        mApplicationContext = context.getApplicationContext();
        enableLegacyMode(false);
        enableQuic(false);
        enableHttp2(true);
        enableSdch(false);
        enableHttpCache(HTTP_CACHE_DISABLED, 0);
        enableNetworkQualityEstimator(false);
        enablePublicKeyPinningBypassForLocalTrustAnchors(true);
    }

    @Override
    public String getDefaultUserAgent() {
        return UserAgent.from(mApplicationContext);
    }

    @Override
    public CronetEngineBuilderImpl setUserAgent(String userAgent) {
        mUserAgent = userAgent;
        return this;
    }

    String getUserAgent() {
        return mUserAgent;
    }

    @Override
    public CronetEngineBuilderImpl setStoragePath(String value) {
        if (!new File(value).isDirectory()) {
            throw new IllegalArgumentException("Storage path must be set to existing directory");
        }
        mStoragePath = value;
        return this;
    }

    String storagePath() {
        return mStoragePath;
    }

    @Override
    public CronetEngineBuilderImpl enableLegacyMode(boolean value) {
        mLegacyModeEnabled = value;
        return this;
    }

    private boolean legacyMode() {
        return mLegacyModeEnabled;
    }

    @Override
    public CronetEngineBuilderImpl setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
        mLibraryLoader = new VersionSafeCallbacks.LibraryLoader(loader);
        return this;
    }

    VersionSafeCallbacks.LibraryLoader libraryLoader() {
        return mLibraryLoader;
    }

    @Override
    public CronetEngineBuilderImpl enableQuic(boolean value) {
        mQuicEnabled = value;
        return this;
    }

    boolean quicEnabled() {
        return mQuicEnabled;
    }

    /**
     * Constructs default QUIC User Agent Id string including application name
     * and Cronet version. Returns empty string if QUIC is not enabled.
     *
     * @return QUIC User Agent ID string.
     */
    String getDefaultQuicUserAgentId() {
        return mQuicEnabled ? UserAgent.getQuicUserAgentIdFrom(mApplicationContext) : "";
    }

    @Override
    public CronetEngineBuilderImpl enableHttp2(boolean value) {
        mHttp2Enabled = value;
        return this;
    }

    boolean http2Enabled() {
        return mHttp2Enabled;
    }

    @Override
    public CronetEngineBuilderImpl enableSdch(boolean value) {
        mSdchEnabled = value;
        return this;
    }

    boolean sdchEnabled() {
        return mSdchEnabled;
    }

    @Override
    public CronetEngineBuilderImpl enableDataReductionProxy(String key) {
        mDataReductionProxyKey = key;
        return this;
    }

    String dataReductionProxyKey() {
        return mDataReductionProxyKey;
    }

    @Override
    public CronetEngineBuilderImpl setDataReductionProxyOptions(
            String primaryProxy, String fallbackProxy, String secureProxyCheckUrl) {
        if (primaryProxy.isEmpty() || fallbackProxy.isEmpty() || secureProxyCheckUrl.isEmpty()) {
            throw new IllegalArgumentException(
                    "Primary and fallback proxies and check url must be set");
        }
        mDataReductionProxyPrimaryProxy = primaryProxy;
        mDataReductionProxyFallbackProxy = fallbackProxy;
        mDataReductionProxySecureProxyCheckUrl = secureProxyCheckUrl;
        return this;
    }

    String dataReductionProxyPrimaryProxy() {
        return mDataReductionProxyPrimaryProxy;
    }

    String dataReductionProxyFallbackProxy() {
        return mDataReductionProxyFallbackProxy;
    }

    String dataReductionProxySecureProxyCheckUrl() {
        return mDataReductionProxySecureProxyCheckUrl;
    }

    @IntDef({
            HTTP_CACHE_DISABLED, HTTP_CACHE_IN_MEMORY, HTTP_CACHE_DISK_NO_HTTP, HTTP_CACHE_DISK,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HttpCacheSetting {}

    @Override
    public CronetEngineBuilderImpl enableHttpCache(@HttpCacheSetting int cacheMode, long maxSize) {
        if (cacheMode == HTTP_CACHE_DISK || cacheMode == HTTP_CACHE_DISK_NO_HTTP) {
            if (storagePath() == null) {
                throw new IllegalArgumentException("Storage path must be set");
            }
        } else {
            if (storagePath() != null) {
                throw new IllegalArgumentException("Storage path must not be set");
            }
        }
        mDisableCache = (cacheMode == HTTP_CACHE_DISABLED || cacheMode == HTTP_CACHE_DISK_NO_HTTP);
        mHttpCacheMaxSize = maxSize;

        switch (cacheMode) {
            case HTTP_CACHE_DISABLED:
                mHttpCacheMode = HttpCacheType.DISABLED;
                break;
            case HTTP_CACHE_DISK_NO_HTTP:
            case HTTP_CACHE_DISK:
                mHttpCacheMode = HttpCacheType.DISK;
                break;
            case HTTP_CACHE_IN_MEMORY:
                mHttpCacheMode = HttpCacheType.MEMORY;
                break;
            default:
                throw new IllegalArgumentException("Unknown cache mode");
        }
        return this;
    }

    boolean cacheDisabled() {
        return mDisableCache;
    }

    long httpCacheMaxSize() {
        return mHttpCacheMaxSize;
    }

    int httpCacheMode() {
        return mHttpCacheMode;
    }

    @Override
    public CronetEngineBuilderImpl addQuicHint(String host, int port, int alternatePort) {
        if (host.contains("/")) {
            throw new IllegalArgumentException("Illegal QUIC Hint Host: " + host);
        }
        mQuicHints.add(new QuicHint(host, port, alternatePort));
        return this;
    }

    List<QuicHint> quicHints() {
        return mQuicHints;
    }

    @Override
    public CronetEngineBuilderImpl addPublicKeyPins(String hostName, Set<byte[]> pinsSha256,
            boolean includeSubdomains, Date expirationDate) {
        if (hostName == null) {
            throw new NullPointerException("The hostname cannot be null");
        }
        if (pinsSha256 == null) {
            throw new NullPointerException("The set of SHA256 pins cannot be null");
        }
        if (expirationDate == null) {
            throw new NullPointerException("The pin expiration date cannot be null");
        }
        String idnHostName = validateHostNameForPinningAndConvert(hostName);
        // Convert the pin to BASE64 encoding. The hash set will eliminate duplications.
        Set<byte[]> hashes = new HashSet<>(pinsSha256.size());
        for (byte[] pinSha256 : pinsSha256) {
            if (pinSha256 == null || pinSha256.length != 32) {
                throw new IllegalArgumentException("Public key pin is invalid");
            }
            hashes.add(pinSha256);
        }
        // Add new element to PKP list.
        mPkps.add(new Pkp(idnHostName, hashes.toArray(new byte[hashes.size()][]), includeSubdomains,
                expirationDate));
        return this;
    }

    /**
     * Returns list of public key pins.
     * @return list of public key pins.
     */
    List<Pkp> publicKeyPins() {
        return mPkps;
    }

    @Override
    public CronetEngineBuilderImpl enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
        mPublicKeyPinningBypassForLocalTrustAnchorsEnabled = value;
        return this;
    }

    boolean publicKeyPinningBypassForLocalTrustAnchorsEnabled() {
        return mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
    }

    /**
     * Checks whether a given string represents a valid host name for PKP and converts it
     * to ASCII Compatible Encoding representation according to RFC 1122, RFC 1123 and
     * RFC 3490. This method is more restrictive than required by RFC 7469. Thus, a host
     * that contains digits and the dot character only is considered invalid.
     *
     * Note: Currently Cronet doesn't have native implementation of host name validation that
     *       can be used. There is code that parses a provided URL but doesn't ensure its
     *       correctness. The implementation relies on {@code getaddrinfo} function.
     *
     * @param hostName host name to check and convert.
     * @return true if the string is a valid host name.
     * @throws IllegalArgumentException if the the given string does not represent a valid
     *                                  hostname.
     */
    private static String validateHostNameForPinningAndConvert(String hostName)
            throws IllegalArgumentException {
        if (INVALID_PKP_HOST_NAME.matcher(hostName).matches()) {
            throw new IllegalArgumentException("Hostname " + hostName + " is illegal."
                    + " A hostname should not consist of digits and/or dots only.");
        }
        // Workaround for crash, see crbug.com/634914
        if (hostName.length() > 255) {
            throw new IllegalArgumentException("Hostname " + hostName + " is too long."
                    + " The name of the host does not comply with RFC 1122 and RFC 1123.");
        }
        try {
            return IDN.toASCII(hostName, IDN.USE_STD3_ASCII_RULES);
        } catch (IllegalArgumentException ex) {
            throw new IllegalArgumentException("Hostname " + hostName + " is illegal."
                    + " The name of the host does not comply with RFC 1122 and RFC 1123.");
        }
    }

    @Override
    public CronetEngineBuilderImpl setExperimentalOptions(String options) {
        mExperimentalOptions = options;
        return this;
    }

    public String experimentalOptions() {
        return mExperimentalOptions;
    }

    /**
     * Sets a native MockCertVerifier for testing. See
     * {@code MockCertVerifier.createMockCertVerifier} for a method that
     * can be used to create a MockCertVerifier.
     * @param mockCertVerifier pointer to native MockCertVerifier.
     * @return the builder to facilitate chaining.
     */
    @VisibleForTesting
    public CronetEngineBuilderImpl setMockCertVerifierForTesting(long mockCertVerifier) {
        mMockCertVerifier = mockCertVerifier;
        return this;
    }

    long mockCertVerifier() {
        return mMockCertVerifier;
    }

    /**
     * @return true if the network quality estimator has been enabled for
     * this builder.
     */
    boolean networkQualityEstimatorEnabled() {
        return mNetworkQualityEstimatorEnabled;
    }

    @Override
    public CronetEngineBuilderImpl setCertVerifierData(String certVerifierData) {
        mCertVerifierData = certVerifierData;
        return this;
    }

    @Override
    public CronetEngineBuilderImpl enableNetworkQualityEstimator(boolean value) {
        mNetworkQualityEstimatorEnabled = value;
        return this;
    }

    String certVerifierData() {
        return mCertVerifierData;
    }

    /**
     * Returns {@link Context} for builder.
     *
     * @return {@link Context} for builder.
     */
    Context getContext() {
        return mApplicationContext;
    }

    @Override
    public ExperimentalCronetEngine build() {
        final ClassLoader loader = getClass().getClassLoader();
        if (getUserAgent() == null) {
            setUserAgent(getDefaultUserAgent());
        }

        ExperimentalCronetEngine cronetEngine = null;
        if (!legacyMode()) {
            cronetEngine = createNativeCronetEngine(loader);
        }
        if (cronetEngine == null) {
            cronetEngine = createJavaCronetEngine(loader);
        }

        if (cronetEngine == null) {
            Log.i(TAG,
                    "Class loader " + loader + " couldn't find any Cronet engine implementation.");
        } else {
            Log.i(TAG, loader.toString() + " found Cronet engine implementation "
                            + cronetEngine.getClass() + ". Network stack version "
                            + cronetEngine.getVersionString());
            // Clear MOCK_CERT_VERIFIER reference if there is any, since
            // the ownership has been transferred to the engine.
            mMockCertVerifier = 0;
        }
        return cronetEngine;
    }

    private ExperimentalCronetEngine createNativeCronetEngine(ClassLoader loader) {
        return createCronetEngine(loader, NATIVE_CRONET_IMPL_CLASS, this);
    }

    private ExperimentalCronetEngine createJavaCronetEngine(ClassLoader loader) {
        return createCronetEngine(loader, JAVA_CRONET_IMPL_CLASS, getUserAgent());
    }

    private ExperimentalCronetEngine createCronetEngine(
            ClassLoader loader, String name, Object argument) {
        ExperimentalCronetEngine cronetEngine = null;
        try {
            Class<? extends ExperimentalCronetEngine> engineClass =
                    loader.loadClass(name).asSubclass(ExperimentalCronetEngine.class);
            Constructor<? extends ExperimentalCronetEngine> constructor =
                    engineClass.getConstructor(argument.getClass());
            cronetEngine = constructor.newInstance(argument);
        } catch (ClassNotFoundException e) {
            // Leave as null.
            Log.i(TAG, "Class loader " + loader + " cannot find Cronet engine implementation: "
                            + name + ". Will try to find an alternative implementation.");
        } catch (Exception e) {
            throw new IllegalStateException("Cannot instantiate: " + name, e);
        }
        return cronetEngine;
    }
}
