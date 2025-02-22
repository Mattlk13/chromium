// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus.CategoryStatusEnum;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the snippets to display on the NTP using the C++ ContentSuggestionsService.
 */
public class SnippetsBridge implements SuggestionsSource {
    private static final String TAG = "SnippetsBridge";

    private long mNativeSnippetsBridge;
    private SuggestionsSource.Observer mObserver;

    public static boolean isCategoryStatusAvailable(@CategoryStatusEnum int status) {
        // Note: This code is duplicated in content_suggestions_category_status.cc.
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.AVAILABLE;
    }

    /** Returns whether the category is considered "enabled", and can show content suggestions. */
    public static boolean isCategoryEnabled(@CategoryStatusEnum int status) {
        switch (status) {
            case CategoryStatus.INITIALIZING:
            case CategoryStatus.AVAILABLE:
            case CategoryStatus.AVAILABLE_LOADING:
            case CategoryStatus.SIGNED_OUT:
                return true;
        }
        return false;
    }

    public static boolean isCategoryLoading(@CategoryStatusEnum int status) {
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.INITIALIZING;
    }

    /**
     * Creates a SnippetsBridge for getting snippet data for the current user.
     *
     * @param profile Profile of the user that we will retrieve snippets for.
     */
    public SnippetsBridge(Profile profile) {
        mNativeSnippetsBridge = nativeInit(profile);
    }

    /**
     * Destroys the native bridge. This object can no longer be used to send native commands, and
     * any observer is nulled out and will stop receiving updates. This object should be discarded.
     */
    public void destroy() {
        assert mNativeSnippetsBridge != 0;
        nativeDestroy(mNativeSnippetsBridge);
        mNativeSnippetsBridge = 0;
        mObserver = null;
    }

    /**
     * Reschedules the fetching of snippets.
     */
    public static void rescheduleFetching() {
        nativeRemoteSuggestionsSchedulerRescheduleFetching();
    }

    /**
     * Fetches remote suggestions in background.
     */
    public static void fetchRemoteSuggestionsFromBackground() {
        nativeRemoteSuggestionsSchedulerOnFetchDue();
    }

    @Override
    public void fetchRemoteSuggestions() {
        nativeReloadSuggestions(mNativeSnippetsBridge);
    }

    @Override
    public int[] getCategories() {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategories(mNativeSnippetsBridge);
    }

    @Override
    @CategoryStatusEnum
    public int getCategoryStatus(int category) {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategoryStatus(mNativeSnippetsBridge, category);
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        assert mNativeSnippetsBridge != 0;
        return nativeGetCategoryInfo(mNativeSnippetsBridge, category);
    }

    @Override
    public List<SnippetArticle> getSuggestionsForCategory(int category) {
        assert mNativeSnippetsBridge != 0;
        return nativeGetSuggestionsForCategory(mNativeSnippetsBridge, category);
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {
        assert mNativeSnippetsBridge != 0;
        nativeFetchSuggestionImage(mNativeSnippetsBridge, suggestion.mCategory,
                suggestion.mIdWithinCategory, callback);
    }

    @Override
    public void dismissSuggestion(SnippetArticle suggestion) {
        assert mNativeSnippetsBridge != 0;
        nativeDismissSuggestion(mNativeSnippetsBridge, suggestion.mUrl, suggestion.mGlobalPosition,
                suggestion.mCategory, suggestion.mPosition, suggestion.mIdWithinCategory);
    }

    @Override
    public void dismissCategory(@CategoryInt int category) {
        assert mNativeSnippetsBridge != 0;
        nativeDismissCategory(mNativeSnippetsBridge, category);
    }

    @Override
    public void restoreDismissedCategories() {
        assert mNativeSnippetsBridge != 0;
        nativeRestoreDismissedCategories(mNativeSnippetsBridge);
    }

    public void onPageShown(int[] categories, int[] suggestionsPerCategory) {
        assert mNativeSnippetsBridge != 0;
        nativeOnPageShown(mNativeSnippetsBridge, categories, suggestionsPerCategory);
    }

    public void onSuggestionShown(SnippetArticle suggestion) {
        assert mNativeSnippetsBridge != 0;
        nativeOnSuggestionShown(mNativeSnippetsBridge, suggestion.mGlobalPosition,
                suggestion.mCategory, suggestion.mPosition,
                suggestion.mPublishTimestampMilliseconds, suggestion.mScore);
    }

    public void onSuggestionOpened(SnippetArticle suggestion, int windowOpenDisposition) {
        assert mNativeSnippetsBridge != 0;
        nativeOnSuggestionOpened(mNativeSnippetsBridge, suggestion.mGlobalPosition,
                suggestion.mCategory, suggestion.mPosition,
                suggestion.mPublishTimestampMilliseconds, suggestion.mScore, windowOpenDisposition);
    }

    public void onSuggestionMenuOpened(SnippetArticle suggestion) {
        assert mNativeSnippetsBridge != 0;
        nativeOnSuggestionMenuOpened(mNativeSnippetsBridge, suggestion.mGlobalPosition,
                suggestion.mCategory, suggestion.mPosition,
                suggestion.mPublishTimestampMilliseconds, suggestion.mScore);
    }

    public void onMoreButtonShown(int category, int position) {
        assert mNativeSnippetsBridge != 0;
        nativeOnMoreButtonShown(mNativeSnippetsBridge, category, position);
    }

    public void onMoreButtonClicked(int category, int position) {
        assert mNativeSnippetsBridge != 0;
        nativeOnMoreButtonClicked(mNativeSnippetsBridge, category, position);
    }

    /**
     * Notifies the scheduler to adjust the plan due to a newly opened NTP.
     */
    public void onNtpInitialized() {
        assert mNativeSnippetsBridge != 0;
        nativeOnNTPInitialized(mNativeSnippetsBridge);
    }

    public static void notifySchedulerAboutWarmResume() {
        SnippetsBridge snippetsBridge = new SnippetsBridge(Profile.getLastUsedProfile());
        snippetsBridge.onActivityWarmResumed();
    }

    public static void notifySchedulerAboutColdStart() {
        SnippetsBridge snippetsBridge = new SnippetsBridge(Profile.getLastUsedProfile());
        snippetsBridge.onColdStart();
    }

    public static void onSuggestionTargetVisited(int category, long visitTimeMs) {
        nativeOnSuggestionTargetVisited(category, visitTimeMs);
    }

    /**
     * Sets the recipient for the fetched snippets.
     *
     * An observer needs to be set before the native code attempts to transmit snippets them to
     * java. Upon registration, the observer will be notified of already fetched snippets.
     *
     * @param observer object to notify when snippets are received.
     */
    @Override
    public void setObserver(SuggestionsSource.Observer observer) {
        assert observer != null;
        mObserver = observer;
    }

    @Override
    public void fetchSuggestions(@CategoryInt int category, String[] displayedSuggestionIds) {
        nativeFetch(mNativeSnippetsBridge, category, displayedSuggestionIds);
    }

    private void onActivityWarmResumed() {
        assert mNativeSnippetsBridge != 0;
        nativeOnActivityWarmResumed(mNativeSnippetsBridge);
    }

    private void onColdStart() {
        assert mNativeSnippetsBridge != 0;
        nativeOnColdStart(mNativeSnippetsBridge);
    }

    @CalledByNative
    private static List<SnippetArticle> createSuggestionList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static SnippetArticle addSuggestion(List<SnippetArticle> suggestions, int category,
            String id, String title, String publisher, String previewText, String url,
            long timestamp, float score) {
        int position = suggestions.size();
        suggestions.add(new SnippetArticle(
                category, id, title, publisher, previewText, url, timestamp, score, position));
        return suggestions.get(position);
    }

    @CalledByNative
    private static void setAssetDownloadDataForSuggestion(
            SnippetArticle suggestion, String filePath, String mimeType) {
        suggestion.setAssetDownloadData(filePath, mimeType);
    }

    @CalledByNative
    private static void setOfflinePageDownloadDataForSuggestion(
            SnippetArticle suggestion, long offlinePageId) {
        suggestion.setOfflinePageDownloadData(offlinePageId);
    }

    @CalledByNative
    private static void setRecentTabDataForSuggestion(
            SnippetArticle suggestion, String tabId, long offlinePageId) {
        suggestion.setRecentTabData(tabId, offlinePageId);
    }

    @CalledByNative
    private static SuggestionsCategoryInfo createSuggestionsCategoryInfo(int category, String title,
            int cardLayout, boolean hasMoreAction, boolean hasReloadAction,
            boolean hasViewAllAction, boolean showIfEmpty, String noSuggestionsMessage) {
        return new SuggestionsCategoryInfo(category, title, cardLayout, hasMoreAction,
                hasReloadAction, hasViewAllAction, showIfEmpty, noSuggestionsMessage);
    }

    @CalledByNative
    private void onNewSuggestions(@CategoryInt int category) {
        if (mObserver != null) mObserver.onNewSuggestions(category);
    }

    @CalledByNative
    private void onMoreSuggestions(@CategoryInt int category, List<SnippetArticle> suggestions) {
        if (mObserver != null) mObserver.onMoreSuggestions(category, suggestions);
    }

    @CalledByNative
    private void onCategoryStatusChanged(
            @CategoryInt int category, @CategoryStatusEnum int newStatus) {
        if (mObserver != null) mObserver.onCategoryStatusChanged(category, newStatus);
    }

    @CalledByNative
    private void onSuggestionInvalidated(@CategoryInt int category, String idWithinCategory) {
        if (mObserver != null) mObserver.onSuggestionInvalidated(category, idWithinCategory);
    }

    @CalledByNative
    private void onFullRefreshRequired() {
        if (mObserver != null) mObserver.onFullRefreshRequired();
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeNTPSnippetsBridge);
    private native void nativeReloadSuggestions(long nativeNTPSnippetsBridge);
    private static native void nativeRemoteSuggestionsSchedulerOnFetchDue();
    private static native void nativeRemoteSuggestionsSchedulerRescheduleFetching();
    private native int[] nativeGetCategories(long nativeNTPSnippetsBridge);
    private native int nativeGetCategoryStatus(long nativeNTPSnippetsBridge, int category);
    private native SuggestionsCategoryInfo nativeGetCategoryInfo(
            long nativeNTPSnippetsBridge, int category);
    private native List<SnippetArticle> nativeGetSuggestionsForCategory(
            long nativeNTPSnippetsBridge, int category);
    private native void nativeFetchSuggestionImage(long nativeNTPSnippetsBridge, int category,
            String idWithinCategory, Callback<Bitmap> callback);
    private native void nativeFetch(
            long nativeNTPSnippetsBridge, int category, String[] knownSuggestions);
    private native void nativeDismissSuggestion(long nativeNTPSnippetsBridge, String url,
            int globalPosition, int category, int categoryPosition, String idWithinCategory);
    private native void nativeDismissCategory(long nativeNTPSnippetsBridge, int category);
    private native void nativeRestoreDismissedCategories(long nativeNTPSnippetsBridge);
    private native void nativeOnPageShown(
            long nativeNTPSnippetsBridge, int[] categories, int[] suggestionsPerCategory);
    private native void nativeOnSuggestionShown(long nativeNTPSnippetsBridge, int globalPosition,
            int category, int categoryPosition, long publishTimestampMs, float score);
    private native void nativeOnSuggestionOpened(long nativeNTPSnippetsBridge, int globalPosition,
            int category, int categoryPosition, long publishTimestampMs, float score,
            int windowOpenDisposition);
    private native void nativeOnSuggestionMenuOpened(long nativeNTPSnippetsBridge,
            int globalPosition, int category, int categoryPosition, long publishTimestampMs,
            float score);
    private native void nativeOnMoreButtonShown(
            long nativeNTPSnippetsBridge, int category, int position);
    private native void nativeOnMoreButtonClicked(
            long nativeNTPSnippetsBridge, int category, int position);
    private native void nativeOnActivityWarmResumed(long nativeNTPSnippetsBridge);
    private native void nativeOnColdStart(long nativeNTPSnippetsBridge);
    private static native void nativeOnSuggestionTargetVisited(int category, long visitTimeMs);
    private static native void nativeOnNTPInitialized(long nativeNTPSnippetsBridge);
}
