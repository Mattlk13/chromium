// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_absolute_utils.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

namespace {

bool AbsoluteHorizontalNeedsEstimate(const ComputedStyle& style) {
  Length width = style.width();
  return width.isIntrinsic() ||
         (width.isAuto() && (style.left().isAuto() || style.right().isAuto()));
}

bool AbsoluteVerticalNeedsEstimate(const ComputedStyle& style) {
  Length height = style.height();
  return height.isIntrinsic() ||
         (height.isAuto() && (style.top().isAuto() || style.bottom().isAuto()));
}

// Implement absolute horizontal size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
void ComputeAbsoluteHorizontal(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<MinAndMaxContentSizes>& child_minmax,
    NGAbsolutePhysicalPosition* position) {
  NGLogicalSize percentage_logical = space.PercentageResolutionSize();
  NGPhysicalSize percentage_physical =
      percentage_logical.ConvertToPhysical(space.WritingMode());
  LayoutUnit border_left(style.borderLeftWidth());
  LayoutUnit border_right(style.borderRightWidth());
  LayoutUnit padding_left =
      valueForLength(style.paddingLeft(), percentage_logical.inline_size);
  LayoutUnit padding_right =
      valueForLength(style.paddingRight(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_left;
  if (!style.marginLeft().isAuto())
    margin_left =
        valueForLength(style.marginLeft(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_right;
  if (!style.marginRight().isAuto())
    margin_right =
        valueForLength(style.marginRight(), percentage_logical.inline_size);
  Optional<LayoutUnit> left;
  if (!style.left().isAuto())
    left = valueForLength(style.left(), percentage_physical.width);
  Optional<LayoutUnit> right;
  if (!style.right().isAuto())
    right = valueForLength(style.right(), percentage_physical.width);
  LayoutUnit border_padding =
      border_left + border_right + padding_left + padding_right;
  Optional<LayoutUnit> width;
  if (!style.width().isAuto()) {
    if (space.WritingMode() == kHorizontalTopBottom) {
      width = ResolveInlineLength(space, style, child_minmax, style.width(),
                                  LengthResolveType::kContentSize);
    } else {
      LayoutUnit computed_width =
          child_minmax.has_value() ? child_minmax->max_content : LayoutUnit();
      width = ResolveBlockLength(space, style, style.width(), computed_width,
                                 LengthResolveType::kContentSize);
    }
  }

  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.WritingMode());
  DCHECK(container_size.width != NGSizeIndefinite);

  // Solving the equation:
  // left + marginLeft + width + marginRight + right  = container width
  if (!left && !right && !width) {
    // Standard: "If all three of left, width, and right are auto:"
    if (!margin_left)
      margin_left = LayoutUnit();
    if (!margin_right)
      margin_right = LayoutUnit();
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(container_size.width);
    if (space.Direction() == TextDirection::kLtr) {
      left = static_position.LeftPosition(container_size.width, *width,
                                          *margin_left, *margin_right);
    } else {
      right = static_position.RightPosition(container_size.width, *width,
                                            *margin_left, *margin_right);
    }
  } else if (left && right && width) {
    // Standard: "If left, right, and width are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.width - *left - *right - *width;
    // When both margins are auto.
    if (!margin_left && !margin_right) {
      if (margin_space > 0) {
        margin_left = margin_space / 2;
        margin_right = margin_space / 2;
      } else {
        // Margins are negative.
        if (space.Direction() == TextDirection::kLtr) {
          margin_left = LayoutUnit();
          margin_right = margin_space;
        } else {
          margin_right = LayoutUnit();
          margin_left = margin_space;
        }
      }
    } else if (!margin_left) {
      margin_left = margin_space;
    } else if (!margin_right) {
      margin_right = margin_space;
    } else {
      // Are values overconstrained?
      if (*margin_left + *margin_right != margin_space) {
        // Relax the end.
        if (space.Direction() == TextDirection::kLtr)
          right = *right - *margin_left + *margin_right - margin_space;
        else
          left = *left - *margin_left + *margin_right - margin_space;
      }
    }
  }

  // Set unknown margins.
  if (!margin_left)
    margin_left = LayoutUnit();
  if (!margin_right)
    margin_right = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown.
  if (!left && !width) {
    // Rule 1: left/width are unknown.
    DCHECK(right.has_value());
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(container_size.width);
  } else if (!left && !right) {
    // Rule 2.
    DCHECK(width.has_value());
    if (space.Direction() == TextDirection::kLtr)
      left = static_position.LeftPosition(container_size.width, *width,
                                          *margin_left, *margin_right);
    else
      right = static_position.RightPosition(container_size.width, *width,
                                            *margin_left, *margin_right);
  } else if (!width && !right) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    width = child_minmax->ShrinkToFit(container_size.width);
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!left) {
    left =
        container_size.width - *right - *width - *margin_left - *margin_right;
  } else if (!right) {
    right =
        container_size.width - *left - *width - *margin_left - *margin_right;
  } else if (!width) {
    width =
        container_size.width - *left - *right - *margin_left - *margin_right;
  }
  DCHECK_EQ(container_size.width,
            *left + *right + *margin_left + *margin_right + *width);

  // Negative widths are not allowed.
  width = std::max(*width, border_padding);

  position->inset.left = *left + *margin_left;
  position->inset.right = *right + *margin_right;
  position->size.width = *width;
}

// Implements absolute vertical size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-height
void ComputeAbsoluteVertical(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<MinAndMaxContentSizes>& child_minmax,
    NGAbsolutePhysicalPosition* position) {
  NGLogicalSize percentage_logical = space.PercentageResolutionSize();
  NGPhysicalSize percentage_physical =
      percentage_logical.ConvertToPhysical(space.WritingMode());

  LayoutUnit border_top(style.borderTopWidth());
  LayoutUnit border_bottom(style.borderBottomWidth());
  LayoutUnit padding_top =
      valueForLength(style.paddingTop(), percentage_logical.inline_size);
  LayoutUnit padding_bottom =
      valueForLength(style.paddingBottom(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_top;
  if (!style.marginTop().isAuto())
    margin_top =
        valueForLength(style.marginTop(), percentage_logical.inline_size);
  Optional<LayoutUnit> margin_bottom;
  if (!style.marginBottom().isAuto())
    margin_bottom =
        valueForLength(style.marginBottom(), percentage_logical.inline_size);
  Optional<LayoutUnit> top;
  if (!style.top().isAuto())
    top = valueForLength(style.top(), percentage_physical.height);
  Optional<LayoutUnit> bottom;
  if (!style.bottom().isAuto())
    bottom = valueForLength(style.bottom(), percentage_physical.height);
  LayoutUnit border_padding =
      border_top + border_bottom + padding_top + padding_bottom;

  Optional<LayoutUnit> height;
  if (!style.height().isAuto()) {
    if (space.WritingMode() == kHorizontalTopBottom) {
      LayoutUnit computed_height =
          child_minmax.has_value() ? child_minmax->max_content : LayoutUnit();
      height = ResolveBlockLength(space, style, style.height(), computed_height,
                                  LengthResolveType::kContentSize);
    } else {
      height = ResolveInlineLength(space, style, child_minmax, style.height(),
                                   LengthResolveType::kContentSize);
    }
  }

  NGPhysicalSize container_size =
      space.AvailableSize().ConvertToPhysical(space.WritingMode());
  DCHECK(container_size.height != NGSizeIndefinite);

  // Solving the equation:
  // top + marginTop + height + marginBottom + bottom
  // + border_padding = container height
  if (!top && !bottom && !height) {
    // Standard: "If all three of top, height, and bottom are auto:"
    if (!margin_top)
      margin_top = LayoutUnit();
    if (!margin_bottom)
      margin_bottom = LayoutUnit();
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(container_size.height);
    top = static_position.TopPosition(container_size.height, *height,
                                      *margin_top, *margin_bottom);
  } else if (top && bottom && height) {
    // Standard: "If top, bottom, and height are not auto:"
    // Compute margins.
    LayoutUnit margin_space = container_size.height - *top - *bottom - *height;
    // When both margins are auto.
    if (!margin_top && !margin_bottom) {
      if (margin_space > 0) {
        margin_top = margin_space / 2;
        margin_bottom = margin_space / 2;
      } else {
        // Margin space is over-constrained.
        margin_top = LayoutUnit();
        margin_bottom = margin_space;
      }
    } else if (!margin_top) {
      margin_top = margin_space;
    } else if (!margin_bottom) {
      margin_bottom = margin_space;
    } else {
      // Are values overconstrained?
      if (*margin_top + *margin_bottom != margin_space) {
        // Relax the end.
        bottom = *bottom - *margin_top + *margin_bottom - margin_space;
      }
    }
  }

  // Set unknown margins.
  if (!margin_top)
    margin_top = LayoutUnit();
  if (!margin_bottom)
    margin_bottom = LayoutUnit();

  // Rules 1 through 3, 2 out of 3 are unknown, fix 1.
  if (!top && !height) {
    // Rule 1.
    DCHECK(bottom.has_value());
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(container_size.height);
  } else if (!top && !bottom) {
    // Rule 2.
    DCHECK(height.has_value());
    top = static_position.TopPosition(container_size.height, *height,
                                      *margin_top, *margin_bottom);
  } else if (!height && !bottom) {
    // Rule 3.
    DCHECK(child_minmax.has_value());
    height = child_minmax->ShrinkToFit(container_size.height);
  }

  // Rules 4 through 6, 1 out of 3 are unknown.
  if (!top) {
    top = container_size.height - *bottom - *height - *margin_top -
          *margin_bottom;
  } else if (!bottom) {
    bottom =
        container_size.height - *top - *height - *margin_top - *margin_bottom;
  } else if (!height) {
    height =
        container_size.height - *top - *bottom - *margin_top - *margin_bottom;
  }
  DCHECK_EQ(container_size.height,
            *top + *bottom + *margin_top + *margin_bottom + *height);

  // Negative heights are not allowed.
  height = std::max(*height, border_padding);

  position->inset.top = *top + *margin_top;
  position->inset.bottom = *bottom + *margin_bottom;
  position->size.height = *height;
}

}  // namespace

String NGAbsolutePhysicalPosition::ToString() const {
  return String::format("INSET(LRTB):%d,%d,%d,%d SIZE:%dx%d",
                        inset.left.toInt(), inset.right.toInt(),
                        inset.top.toInt(), inset.bottom.toInt(),
                        size.width.toInt(), size.height.toInt());
}

bool AbsoluteNeedsChildBlockSize(const ComputedStyle& style) {
  if (style.isHorizontalWritingMode())
    return AbsoluteVerticalNeedsEstimate(style);
  else
    return AbsoluteHorizontalNeedsEstimate(style);
}

bool AbsoluteNeedsChildInlineSize(const ComputedStyle& style) {
  if (style.isHorizontalWritingMode())
    return AbsoluteHorizontalNeedsEstimate(style);
  else
    return AbsoluteVerticalNeedsEstimate(style);
}

NGAbsolutePhysicalPosition ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<MinAndMaxContentSizes>& child_minmax) {
  NGAbsolutePhysicalPosition position;
  if (style.isHorizontalWritingMode())
    ComputeAbsoluteHorizontal(space, style, static_position, child_minmax,
                              &position);
  else {
    ComputeAbsoluteVertical(space, style, static_position, child_minmax,
                            &position);
  }
  return position;
}

void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition& static_position,
    const Optional<LayoutUnit>& child_block_size,
    NGAbsolutePhysicalPosition* position) {
  // After partial size has been computed, child block size is either
  // unknown, or fully computed, there is no minmax.
  // To express this, a 'fixed' minmax is created where
  // min and max are the same.
  Optional<MinAndMaxContentSizes> child_minmax;
  if (child_block_size.has_value()) {
    child_minmax = MinAndMaxContentSizes{*child_block_size, *child_block_size};
  }
  if (style.isHorizontalWritingMode())
    ComputeAbsoluteVertical(space, style, static_position, child_minmax,
                            position);
  else {
    ComputeAbsoluteHorizontal(space, style, static_position, child_minmax,
                              position);
  }
}

}  // namespace blink
