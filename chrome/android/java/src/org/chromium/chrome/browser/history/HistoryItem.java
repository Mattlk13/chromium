// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.text.TextUtils;

import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;

import java.util.Arrays;

/** Contains information about a single browsing history item. */
public class HistoryItem extends TimedItem {
    private final String mUrl;
    private final String mDomain;
    private final String mTitle;
    private final long mTimestamp;
    private final long[] mTimestampList;
    private Long mStableId;

    private HistoryManager mManager;

    /**
     * @param url The url for this item.
     * @param domain The string to display for the item's domain.
     * @param title The string to display for the item's title.
     * @param timestamps The list of timestamps for this item.
     */
    public HistoryItem(String url, String domain, String title, long[] timestamps) {
        mUrl = url;
        mDomain = domain;
        mTitle = TextUtils.isEmpty(title) ? url : title;
        mTimestampList = Arrays.copyOf(timestamps, timestamps.length);

        // The last timestamp in the list is the most recent visit.
        mTimestamp = mTimestampList[mTimestampList.length - 1];
    }

    /** @return The url for this item. */
    public String getUrl() {
        return mUrl;
    }

    /** @return The string to display for the item's domain. */
    public String getDomain() {
        return mDomain;
    }

    /** @return The string to display for the item's title. */
    public String getTitle() {
        return mTitle;
    }

    @Override
    public long getTimestamp() {
        return mTimestamp;
    }

    /**
     * @return An array of timestamps representing visits to this item's url.
     */
    public long[] getTimestamps() {
        return Arrays.copyOf(mTimestampList, mTimestampList.length);
    }

    @Override
    public long getStableId() {
        if (mStableId == null) {
            // Generate a stable ID that combines the timestamp and the URL.
            mStableId = (long) mUrl.hashCode();
            mStableId = (mStableId << 32) + (getTimestamp() & 0x0FFFFFFFF);
        }
        return mStableId;
    }

    /**
     * @param manager The HistoryManager associated with this item.
     */
    public void setHistoryManager(HistoryManager manager) {
        mManager = manager;
    }

    /**
     * Navigates a tab to this item's URL.
     */
    public void open() {
        if (mManager != null) {
            mManager.recordUserActionWithOptionalSearch("OpenItem");
            mManager.openUrl(mUrl, null, false);
        }
    }

    /**
     * Removes this item.
     */
    public void remove() {
        if (mManager != null) {
            mManager.recordUserActionWithOptionalSearch("RemoveItem");
            mManager.removeItem(this);
        }
    }
}
