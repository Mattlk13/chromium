// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;

import java.util.List;
import java.util.Set;

/**
 * Dropdown item adapter for DropdownPopupWindow.
 */
public class DropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final Set<Integer> mSeparators;
    private final boolean mAreAllItemsEnabled;
    private final Integer mBackgroundColor;
    private final Integer mDividerColor;
    private final Integer mDropdownItemHeight;

    /**
     * Creates an {@code ArrayAdapter} with specified parameters.
     * @param context Application context.
     * @param items List of labels and icons to display.
     * @param separators Set of positions that separate {@code items}.
     * @param backgroundColor Popup background color, or {@code null} to use default background
     * color. The default color is {@code Color.TRANSPARENT}.
     * @param dividerColor If {@code null}, use the values in colors.xml for the divider
     * between items. Otherwise, uses {@param dividerColor} for the divider between items. Always
     * uses the values in colors.xml for the dark divider for the separators.
     * @param dropdownItemHeight If {@code null}, uses the {@code dropdown_item_height} in
     * dimens.xml. Otherwise, uses {@param dropdownItemHeight}.
     */
    public DropdownAdapter(Context context, List<? extends DropdownItem> items,
            Set<Integer> separators, Integer backgroundColor, Integer dividerColor,
            Integer dropdownItemHeight) {
        super(context, R.layout.dropdown_item);
        mContext = context;
        addAll(items);
        mSeparators = separators;
        mAreAllItemsEnabled = checkAreAllItemsEnabled();
        mBackgroundColor = backgroundColor;
        mDividerColor = dividerColor;
        mDropdownItemHeight = dropdownItemHeight;
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = getItem(i);
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            layout = inflater.inflate(R.layout.dropdown_item, null);
            layout.setBackground(new DropdownDividerDrawable(mBackgroundColor));
        }
        DropdownDividerDrawable divider = (DropdownDividerDrawable) layout.getBackground();
        int height;
        if (mDropdownItemHeight == null) {
            height = mContext.getResources().getDimensionPixelSize(R.dimen.dropdown_item_height);
        } else {
            height = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                    mDropdownItemHeight, mContext.getResources().getDisplayMetrics());
        }

        if (position == 0) {
            divider.setDividerColor(Color.TRANSPARENT);
        } else {
            int dividerHeight = mContext.getResources().getDimensionPixelSize(
                    R.dimen.dropdown_item_divider_height);
            height += dividerHeight;
            divider.setHeight(dividerHeight);
            int dividerColor;
            if (mSeparators != null && mSeparators.contains(position)) {
                dividerColor = ApiCompatibilityUtils.getColor(mContext.getResources(),
                        R.color.dropdown_dark_divider_color);
            } else if (mDividerColor == null) {
                dividerColor = ApiCompatibilityUtils.getColor(mContext.getResources(),
                        R.color.dropdown_divider_color);
            } else {
                dividerColor = mDividerColor;
            }
            divider.setDividerColor(dividerColor);
        }

        DropdownItem item = getItem(position);

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews instead.
        // If you need to modify this layout, don't forget to test it with TalkBack and make sure
        // it doesn't regress.
        // http://crbug.com/429364
        LinearLayout wrapper = (LinearLayout) layout.findViewById(R.id.dropdown_label_wrapper);
        if (item.isMultilineLabel()) height = LayoutParams.WRAP_CONTENT;
        if (item.isLabelAndSublabelOnSameLine()) {
            wrapper.setOrientation(LinearLayout.HORIZONTAL);
        } else {
            wrapper.setOrientation(LinearLayout.VERTICAL);
        }

        wrapper.setLayoutParams(new LinearLayout.LayoutParams(0, height, 1));

        TextView labelView = (TextView) layout.findViewById(R.id.dropdown_label);
        labelView.setText(item.getLabel());
        labelView.setSingleLine(!item.isMultilineLabel());

        if (item.isLabelAndSublabelOnSameLine()) {
            labelView.setLayoutParams(
                    new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 1));
        } else {
            LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                    LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            int labelMargin = mContext.getResources().getDimensionPixelSize(
                    R.dimen.dropdown_item_label_margin);
            ApiCompatibilityUtils.setMarginStart(layoutParams, labelMargin);
            ApiCompatibilityUtils.setMarginEnd(layoutParams, labelMargin);
            labelView.setLayoutParams(layoutParams);
        }

        labelView.setEnabled(item.isEnabled());
        if (item.isGroupHeader() || item.isBoldLabel()) {
            labelView.setTypeface(null, Typeface.BOLD);
        } else {
            labelView.setTypeface(null, Typeface.NORMAL);
        }

        labelView.setTextColor(ApiCompatibilityUtils.getColor(
                mContext.getResources(), item.getLabelFontColorResId()));
        labelView.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                mContext.getResources().getDimension(item.getLabelFontSizeResId()));

        TextView sublabelView = (TextView) layout.findViewById(R.id.dropdown_sublabel);
        CharSequence sublabel = item.getSublabel();
        if (TextUtils.isEmpty(sublabel)) {
            sublabelView.setVisibility(View.GONE);
        } else {
            sublabelView.setText(sublabel);
            sublabelView.setVisibility(View.VISIBLE);
        }

        ImageView iconViewStart = (ImageView) layout.findViewById(R.id.start_dropdown_icon);
        ImageView iconViewEnd = (ImageView) layout.findViewById(R.id.end_dropdown_icon);
        if (item.isIconAtStart()) {
            iconViewEnd.setVisibility(View.GONE);
        } else {
            iconViewStart.setVisibility(View.GONE);
        }

        ImageView iconView = item.isIconAtStart() ? iconViewStart : iconViewEnd;
        if (item.getIconId() == DropdownItem.NO_ICON) {
            iconView.setVisibility(View.GONE);
        } else {
            iconView.setImageDrawable(AppCompatResources.getDrawable(mContext, item.getIconId()));
            iconView.setVisibility(View.VISIBLE);
        }

        return layout;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return mAreAllItemsEnabled;
    }

    @Override
    public boolean isEnabled(int position) {
        if (position < 0 || position >= getCount()) return false;
        DropdownItem item = getItem(position);
        return item.isEnabled() && !item.isGroupHeader();
    }
}
