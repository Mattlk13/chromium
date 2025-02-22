// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.widget.DateDividedAdapter;
import org.chromium.chrome.browser.widget.selection.SelectableItemViewHolder;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;
import java.util.Locale;

/**
 * Bridges the user's browsing history and the UI used to display it.
 */
public class HistoryAdapter extends DateDividedAdapter implements BrowsingHistoryObserver {
    private static final String EMPTY_QUERY = "";
    private static final String LEARN_MORE_LINK =
            "https://support.google.com/chrome/?p=sync_history&amp;hl="
                    + Locale.getDefault().toString();
    private static final String GOOGLE_HISTORY_LINK = "history.google.com";

    private final SelectionDelegate<HistoryItem> mSelectionDelegate;
    private final HistoryProvider mHistoryProvider;
    private final HistoryManager mManager;

    private TextView mSignedInNotSyncedTextView;
    private TextView mSignedInSyncedTextView;
    private TextView mOtherFormsOfBrowsingHistoryTextView;

    private boolean mHasOtherFormsOfBrowsingData;
    private boolean mHasSyncedData;
    private boolean mIsHeaderInflated;
    private boolean mIsDestroyed;
    private boolean mIsInitialized;
    private boolean mIsLoadingItems;
    private boolean mIsSearching;
    private boolean mHasMorePotentialItems;
    private boolean mClearOnNextQueryComplete;
    private long mNextQueryEndTime;
    private String mQueryText = EMPTY_QUERY;

    public HistoryAdapter(SelectionDelegate<HistoryItem> delegate, HistoryManager manager,
            HistoryProvider provider) {
        setHasStableIds(true);
        mSelectionDelegate = delegate;
        mHistoryProvider = provider;
        mHistoryProvider.setObserver(this);
        mManager = manager;
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        mHistoryProvider.destroy();
        mIsDestroyed = true;
    }

    /**
     * Initializes the HistoryAdapter and loads the first set of browsing history items.
     */
    public void initialize() {
        mIsInitialized = false;
        mIsLoadingItems = true;
        mNextQueryEndTime = 0;
        mClearOnNextQueryComplete = true;
        mHistoryProvider.queryHistory(mQueryText, mNextQueryEndTime);
    }

    /**
     * Load more browsing history items. Returns early if more items are already being loaded or
     * there are no more items to load.
     */
    public void loadMoreItems() {
        if (!canLoadMoreItems()) return;

        mIsLoadingItems = true;
        addFooter();
        notifyDataSetChanged();
        mHistoryProvider.queryHistory(mQueryText, mNextQueryEndTime);
    }

    /**
     * @return Whether more items can be loaded right now.
     */
    public boolean canLoadMoreItems() {
        return !mIsLoadingItems && mHasMorePotentialItems;
    }

    /**
     * Called to perform a search.
     * @param query The text to search for.
     */
    public void search(String query) {
        mQueryText = query;
        mNextQueryEndTime = 0;
        mIsSearching = true;
        mClearOnNextQueryComplete = true;
        mHistoryProvider.queryHistory(mQueryText, mNextQueryEndTime);
    }

    /**
     * Called when a search is ended.
     */
    public void onEndSearch() {
        mQueryText = EMPTY_QUERY;
        mIsSearching = false;

        // Re-initialize the data in the adapter.
        initialize();
    }

    /**
     * Adds the HistoryItem to the list of items being removed and removes it from the adapter. The
     * removal will not be committed until #removeItems() is called.
     * @param item The item to mark for removal.
     */
    public void markItemForRemoval(HistoryItem item) {
        removeItem(item);
        mHistoryProvider.markItemForRemoval(item);

        // If there is only one item left, remove the header so the empty view will be displayed.
        if (getItemCount() == 1) {
            removeHeader();
        }
    }

    /**
     * Removes all items that have been marked for removal through #markItemForRemoval().
     */
    public void removeItems() {
        mHistoryProvider.removeItems();
    }

    @Override
    protected ViewHolder createViewHolder(ViewGroup parent) {
        View v = LayoutInflater.from(parent.getContext()).inflate(
                R.layout.history_item_view, parent, false);
        return new SelectableItemViewHolder<HistoryItem>(v, mSelectionDelegate);
    }

    @Override
    protected void bindViewHolderForTimedItem(ViewHolder current, TimedItem timedItem) {
        final HistoryItem item = (HistoryItem) timedItem;
        @SuppressWarnings("unchecked")
        SelectableItemViewHolder<HistoryItem> holder =
                (SelectableItemViewHolder<HistoryItem>) current;
        holder.displayItem(item);
        ((HistoryItemView) holder.itemView).setHistoryManager(mManager);
    }

    @Override
    protected int getTimedItemViewResId() {
        return R.layout.date_view;
    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        // Return early if the results are returned after the activity/native page is
        // destroyed to avoid unnecessary work.
        if (mIsDestroyed) return;

        if (mClearOnNextQueryComplete) {
            clear(true);
            mClearOnNextQueryComplete = false;
        }

        if (!mIsInitialized) {
            if (items.size() > 0 && !mIsSearching) addHeader();
            mIsInitialized = true;
        }

        removeFooter();
        loadItems(items);

        mIsLoadingItems = false;
        mHasMorePotentialItems = hasMorePotentialMatches;
        if (items.size() > 0) {
            mNextQueryEndTime = items.get(items.size() - 1).getTimestamp();
        }
    }

    @Override
    public void onHistoryDeleted() {
        mSelectionDelegate.clearSelection();
        // TODO(twellington): Account for items that have been paged in due to infinite scroll.
        //                    This currently removes all items and re-issues a query.
        initialize();
    }

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms, boolean hasSyncedResults) {
        mHasOtherFormsOfBrowsingData = hasOtherForms;
        mHasSyncedData = hasSyncedResults;
        setPrivacyDisclaimerVisibility();
    }

    @Override
    protected BasicViewHolder createHeader(ViewGroup parent) {
        ViewGroup v = (ViewGroup) LayoutInflater.from(parent.getContext()).inflate(
                R.layout.history_header, parent, false);
        mIsHeaderInflated = true;

        View cbdButton = v.findViewById(R.id.clear_browsing_data_button);
        cbdButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.openClearBrowsingDataPreference();
            }
        });

        mSignedInNotSyncedTextView = (TextView) v.findViewById(R.id.signed_in_not_synced);
        setPrivacyDisclaimerText(mSignedInNotSyncedTextView,
                R.string.android_history_no_synced_results, LEARN_MORE_LINK);

        mSignedInSyncedTextView = (TextView) v.findViewById(R.id.signed_in_synced);
        setPrivacyDisclaimerText(mSignedInSyncedTextView,
                R.string.android_history_has_synced_results, LEARN_MORE_LINK);

        mOtherFormsOfBrowsingHistoryTextView = (TextView) v.findViewById(
                R.id.other_forms_of_browsing_history);
        setPrivacyDisclaimerText(mOtherFormsOfBrowsingHistoryTextView,
                R.string.android_history_other_forms_of_history, GOOGLE_HISTORY_LINK);

        setPrivacyDisclaimerVisibility();

        return new BasicViewHolder(v);
    }

    @Override
    protected BasicViewHolder createFooter(ViewGroup parent) {
        return new BasicViewHolder(LayoutInflater.from(parent.getContext()).inflate(
                R.layout.indeterminate_progress_view, parent, false));
    }

    private void setPrivacyDisclaimerText(TextView view, int stringId, final  String url) {
        NoUnderlineClickableSpan link = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View view) {
                mManager.openUrl(url, null, true);
            }
        };
        SpannableString spannable = SpanApplier.applySpans(
                view.getResources().getString(stringId),
                new SpanApplier.SpanInfo("<link>", "</link>", link));
        view.setText(spannable);
        view.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private void setPrivacyDisclaimerVisibility() {
        if (!mIsHeaderInflated) return;

        boolean isSignedIn =
                ChromeSigninController.get(ContextUtils.getApplicationContext()).isSignedIn();
        mSignedInNotSyncedTextView.setVisibility(
                !mHasSyncedData && isSignedIn ? View.VISIBLE : View.GONE);
        mSignedInSyncedTextView.setVisibility(mHasSyncedData ? View.VISIBLE : View.GONE);
        mOtherFormsOfBrowsingHistoryTextView.setVisibility(
                mHasOtherFormsOfBrowsingData ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    TextView getSignedInNotSyncedViewForTests() {
        return mSignedInNotSyncedTextView;
    }

    @VisibleForTesting
    TextView getSignedInSyncedViewForTests() {
        return mSignedInSyncedTextView;
    }

    @VisibleForTesting
    TextView getOtherFormsOfBrowsingHistoryViewForTests() {
        return mOtherFormsOfBrowsingHistoryTextView;
    }
}
