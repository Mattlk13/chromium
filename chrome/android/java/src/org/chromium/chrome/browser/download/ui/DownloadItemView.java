// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadNotificationService;
import org.chromium.chrome.browser.widget.MaterialProgressBar;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.TintedImageView;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.UiUtils;

/**
 * The view for a downloaded item displayed in the Downloads list.
 */
public class DownloadItemView extends SelectableItemView<DownloadHistoryItemWrapper>
        implements ThumbnailProvider.ThumbnailRequest {
    private final int mIconBackgroundColor;
    private final int mIconBackgroundColorSelected;
    private final ColorStateList mWhiteTint;

    private DownloadHistoryItemWrapper mItem;
    private int mIconResId;
    private Bitmap mThumbnailBitmap;

    // Controls common to completed and in-progress downloads.
    private LinearLayout mLayoutContainer;
    private TintedImageView mIconView;

    // Controls for completed downloads.
    private View mLayoutCompleted;
    private TextView mFilenameCompletedView;
    private TextView mHostnameView;
    private TextView mFilesizeView;

    // Controls for in-progress downloads.
    private View mLayoutInProgress;
    private TextView mFilenameInProgressView;
    private TextView mDownloadStatusView;
    private MaterialProgressBar mProgressView;
    private TintedImageButton mPauseResumeButton;
    private View mCancelButton;

    /**
     * Constructor for inflating from XML.
     */
    public DownloadItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIconBackgroundColor =
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.light_active_color);
        mIconBackgroundColorSelected =
                ApiCompatibilityUtils.getColor(context.getResources(), R.color.google_grey_600);
        mWhiteTint =
                ApiCompatibilityUtils.getColorStateList(getResources(), R.color.white_mode_tint);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = (TintedImageView) findViewById(R.id.icon_view);
        mProgressView = (MaterialProgressBar) findViewById(R.id.download_progress_view);

        mLayoutContainer = (LinearLayout) findViewById(R.id.layout_container);
        mLayoutCompleted = findViewById(R.id.completed_layout);
        mLayoutInProgress = findViewById(R.id.progress_layout);

        mFilenameCompletedView = (TextView) findViewById(R.id.filename_completed_view);
        mHostnameView = (TextView) findViewById(R.id.hostname_view);
        mFilesizeView = (TextView) findViewById(R.id.filesize_view);

        mFilenameInProgressView = (TextView) findViewById(R.id.filename_progress_view);
        mDownloadStatusView = (TextView) findViewById(R.id.status_view);

        mPauseResumeButton = (TintedImageButton) findViewById(R.id.pause_button);
        mCancelButton = findViewById(R.id.cancel_button);

        mPauseResumeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mItem.isResumable()) {
                    mItem.resume();
                } else if (!mItem.isComplete()) {
                    mItem.pause();
                }
            }
        });
        mCancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mItem.cancel();
            }
        });
    }

    @Override
    public String getFilePath() {
        return mItem == null ? null : mItem.getFilePath();
    }

    @Override
    public void onThumbnailRetrieved(String filePath, Bitmap thumbnail) {
        if (TextUtils.equals(getFilePath(), filePath) && thumbnail != null
                && thumbnail.getWidth() != 0 && thumbnail.getHeight() != 0) {
            setThumbnailBitmap(thumbnail);
        }
    }

    /**
     * Initialize the DownloadItemView. Must be called before the item can respond to click events.
     *
     * @param provider The BackendProvider that allows interacting with the data backends.
     * @param item     The item represented by this DownloadItemView.
     */
    public void displayItem(BackendProvider provider, DownloadHistoryItemWrapper item) {
        mItem = item;
        setItem(item);

        // Cancel any previous thumbnail request for the previously displayed item.
        ThumbnailProvider thumbnailProvider = provider.getThumbnailProvider();
        thumbnailProvider.cancelRetrieval(this);

        Context context = mFilesizeView.getContext();
        mFilenameCompletedView.setText(item.getDisplayFileName());
        mFilenameInProgressView.setText(item.getDisplayFileName());
        mHostnameView.setText(
                UrlFormatter.formatUrlForSecurityDisplay(item.getUrl(), false));
        mFilesizeView.setText(
                Formatter.formatFileSize(context, item.getFileSize()));

        // Asynchronously grab a thumbnail for the file if it might have one.
        int fileType = item.getFilterType();
        mThumbnailBitmap = null;
        if (fileType == DownloadFilter.FILTER_IMAGE && item.isComplete()) {
            mThumbnailBitmap = thumbnailProvider.getThumbnail(this);
        } else {
            // TODO(dfalcantara): Get thumbnails for audio and video files when possible.
        }

        // Pick what icon to display for the item.
        mIconResId = R.drawable.ic_drive_file_white_24dp;
        switch (fileType) {
            case DownloadFilter.FILTER_PAGE:
                mIconResId = R.drawable.ic_drive_site_white_24dp;
                break;
            case DownloadFilter.FILTER_VIDEO:
                mIconResId = R.drawable.ic_play_arrow_white_24dp;
                break;
            case DownloadFilter.FILTER_AUDIO:
                mIconResId = R.drawable.ic_music_note_white_24dp;
                break;
            case DownloadFilter.FILTER_IMAGE:
                mIconResId = R.drawable.ic_image_white_24dp;
                break;
            case DownloadFilter.FILTER_DOCUMENT:
                mIconResId = R.drawable.ic_drive_text_white_24dp;
                break;
            default:
        }

        updateIconView();

        if (item.isComplete()) {
            showLayout(mLayoutCompleted);
        } else {
            showLayout(mLayoutInProgress);
            mDownloadStatusView.setText(item.getStatusString());

            if (item.isResumable() || item.isPaused()) {
                mPauseResumeButton.setImageResource(R.drawable.ic_media_control_play);
                mPauseResumeButton.setContentDescription(
                        getContext().getString(R.string.download_notification_resume_button));
                mProgressView.setIndeterminate(false);
            } else {
                mPauseResumeButton.setImageResource(R.drawable.ic_media_control_pause);
                mPauseResumeButton.setContentDescription(
                        getContext().getString(R.string.download_notification_pause_button));
                mProgressView.setIndeterminate(item.getDownloadProgress()
                        == DownloadNotificationService.INVALID_DOWNLOAD_PERCENTAGE);
            }
            mProgressView.setProgress(item.getDownloadProgress());
        }
    }

    /**
     * @param thumbnail The Bitmap to use for the icon ImageView.
     */
    public void setThumbnailBitmap(Bitmap thumbnail) {
        mThumbnailBitmap = thumbnail;
        updateIconView();
    }

    @Override
    public void onClick() {
        if (mItem != null && mItem.isComplete()) mItem.open();
    }

    @Override
    public boolean onLongClick(View view) {
        if (mItem != null && mItem.isComplete()) {
            return super.onLongClick(view);
        } else {
            return true;
        }
    }

    @Override
    public void setChecked(boolean checked) {
        super.setChecked(checked);
        updateIconView();
    }

    private void updateIconView() {
        if (isChecked()) {
            mIconView.setBackgroundColor(mIconBackgroundColorSelected);
            mIconView.setImageResource(R.drawable.ic_check_googblue_24dp);
            mIconView.setTint(mWhiteTint);
        } else if (mThumbnailBitmap != null) {
            mIconView.setBackground(null);
            mIconView.setImageBitmap(mThumbnailBitmap);
            mIconView.setTint(null);
        } else {
            mIconView.setBackgroundColor(mIconBackgroundColor);
            mIconView.setImageResource(mIconResId);
            mIconView.setTint(mWhiteTint);
        }
    }

    private void showLayout(View layoutToShow) {
        if (mLayoutCompleted != layoutToShow) UiUtils.removeViewFromParent(mLayoutCompleted);
        if (mLayoutInProgress != layoutToShow) UiUtils.removeViewFromParent(mLayoutInProgress);

        if (layoutToShow.getParent() == null) {
            LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT);
            params.weight = 1;
            mLayoutContainer.addView(layoutToShow, params);
        }
    }
}
