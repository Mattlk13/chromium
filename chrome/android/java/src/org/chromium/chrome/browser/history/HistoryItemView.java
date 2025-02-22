// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;

/**
 * The SelectableItemView for items displayed in the browsing history UI.
 */
public class HistoryItemView extends SelectableItemView<HistoryItem> implements LargeIconCallback {
    private TextView mTitle;
    private TextView mDomain;
    private TintedImageButton mRemoveButton;
    private ImageView mIconImageView;

    private HistoryManager mHistoryManager;
    private final RoundedIconGenerator mIconGenerator;

    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private final int mCornerRadius;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mCornerRadius = getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        mMinIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        int textSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        int iconColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize , mDisplayedIconSize,
                mCornerRadius, iconColor, textSize);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = (TextView) findViewById(R.id.title);
        mDomain = (TextView) findViewById(R.id.domain);
        mIconImageView = (ImageView) findViewById(R.id.icon_view);
        mRemoveButton = (TintedImageButton) findViewById(R.id.remove);
        mRemoveButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                remove();
            }
        });
    }

    @Override
    public void setItem(HistoryItem item) {
        if (getItem() == item) return;

        super.setItem(item);

        mTitle.setText(item.getTitle());
        mDomain.setText(item.getDomain());
        mIconImageView.setImageResource(R.drawable.default_favicon);
        if (mHistoryManager != null) requestIcon();
    }

    /**
     * @param manager The HistoryManager associated with this item.
     */
    public void setHistoryManager(HistoryManager manager) {
        getItem().setHistoryManager(manager);
        if (mHistoryManager == manager) return;

        mHistoryManager = manager;
        requestIcon();
    }

    /**
     * Removes the item associated with this view.
     */
    public void remove() {
        getItem().remove();
    }

    @Override
    protected void onClick() {
        if (getItem() != null) getItem().open();
    }

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor,
            boolean isFallbackColorDefault) {
        // TODO(twellington): move this somewhere that can be shared with bookmarks.
        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(getItem().getUrl());
            mIconImageView.setImageDrawable(new BitmapDrawable(getResources(), icon));
        } else {
            RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                    getResources(),
                    Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
            roundedIcon.setCornerRadius(mCornerRadius);
            mIconImageView.setImageDrawable(roundedIcon);
        }
    }

    private void requestIcon() {
        mHistoryManager.getLargeIconBridge().getLargeIconForUrl(
                getItem().getUrl(), mMinIconSize, this);
    }
}
