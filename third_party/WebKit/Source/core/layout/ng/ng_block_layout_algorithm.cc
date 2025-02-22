// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_fragment.h"
#include "platform/LengthFunctions.h"
#include "wtf/Optional.h"

namespace blink {
namespace {

// Adjusts content's offset to CSS "clear" property.
// TODO(glebl): Support margin collapsing edge cases, e.g. margin collapsing
// should not occur if "clear" is applied to non-floating blocks.
// TODO(layout-ng): the call to AdjustToClearance should be moved to
// CreateConstraintSpaceForChild once ConstraintSpaceBuilder is sharing the
// exclusion information between constraint spaces.
void AdjustToClearance(const NGConstraintSpace& space,
                       const ComputedStyle& style,
                       LayoutUnit* content_size) {
  const NGExclusion* right_exclusion = space.Exclusions()->last_right_float;
  const NGExclusion* left_exclusion = space.Exclusions()->last_left_float;

  // Calculates Left/Right block end offset from left/right float exclusions or
  // use the default content offset position.
  LayoutUnit left_block_end_offset =
      left_exclusion ? left_exclusion->rect.BlockEndOffset() : *content_size;
  LayoutUnit right_block_end_offset =
      right_exclusion ? right_exclusion->rect.BlockEndOffset() : *content_size;

  switch (style.clear()) {
    case EClear::ClearNone:
      return;  // nothing to do here.
    case EClear::ClearLeft:
      *content_size = left_block_end_offset;
      break;
    case EClear::ClearRight:
      *content_size = right_block_end_offset;
      break;
    case EClear::ClearBoth:
      *content_size = std::max(left_block_end_offset, right_block_end_offset);
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

LayoutUnit ComputeCollapsedMarginBlockStart(
    const NGMarginStrut& prev_margin_strut,
    const NGMarginStrut& curr_margin_strut) {
  return std::max(prev_margin_strut.margin_block_end,
                  curr_margin_strut.margin_block_start) -
         std::max(prev_margin_strut.negative_margin_block_end.abs(),
                  curr_margin_strut.negative_margin_block_start.abs());
}

// Creates an exclusion from the fragment that will be placed in the provided
// layout opportunity.
NGExclusion CreateExclusion(const NGFragment& fragment,
                            const NGLayoutOpportunity& opportunity,
                            LayoutUnit float_offset,
                            NGBoxStrut margins,
                            NGExclusion::Type exclusion_type) {
  NGExclusion exclusion;
  exclusion.type = exclusion_type;
  NGLogicalRect& rect = exclusion.rect;
  rect.offset = opportunity.offset;
  rect.offset.inline_offset += float_offset;

  rect.size.inline_size = fragment.InlineSize();
  rect.size.block_size = fragment.BlockSize();

  // Adjust to child's margin.
  rect.size.block_size += margins.BlockSum();
  rect.size.inline_size += margins.InlineSum();

  return exclusion;
}

// Finds a layout opportunity for the fragment.
// It iterates over all layout opportunities in the constraint space and returns
// the first layout opportunity that is wider than the fragment or returns the
// last one which is always the widest.
//
// @param space Constraint space that is used to find layout opportunity for
//              the fragment.
// @param fragment Fragment that needs to be placed.
// @param margins Margins of the fragment.
// @return Layout opportunity for the fragment.
const NGLayoutOpportunity FindLayoutOpportunityForFragment(
    NGConstraintSpace* space,
    const NGFragment& fragment,
    const NGBoxStrut& margins) {
  NGLayoutOpportunityIterator* opportunity_iter = space->LayoutOpportunities();
  NGLayoutOpportunity opportunity;
  NGLayoutOpportunity opportunity_candidate = opportunity_iter->Next();

  while (!opportunity_candidate.IsEmpty()) {
    opportunity = opportunity_candidate;
    // Checking opportunity's block size is not necessary as a float cannot be
    // positioned on top of another float inside of the same constraint space.
    auto fragment_inline_size = fragment.InlineSize() + margins.InlineSum();
    if (opportunity.size.inline_size > fragment_inline_size)
      break;

    opportunity_candidate = opportunity_iter->Next();
  }

  return opportunity;
}

// Calculates the logical offset for opportunity.
NGLogicalOffset CalculateLogicalOffsetForOpportunity(
    const NGLayoutOpportunity& opportunity,
    LayoutUnit float_offset,
    NGBoxStrut margins) {
  // Adjust to child's margin.
  LayoutUnit inline_offset = margins.inline_start;
  LayoutUnit block_offset = margins.block_start;

  // Offset from the opportunity's block/inline start.
  inline_offset += opportunity.offset.inline_offset;
  block_offset += opportunity.offset.block_offset;

  inline_offset += float_offset;

  return NGLogicalOffset(inline_offset, block_offset);
}

// Whether an in-flow block-level child creates a new formatting context.
//
// This will *NOT* check the following cases:
//  - The child is out-of-flow, e.g. floating or abs-pos.
//  - The child is a inline-level, e.g. "display: inline-block".
//  - The child establishes a new formatting context, but should be a child of
//    another layout algorithm, e.g. "display: table-caption" or flex-item.
bool IsNewFormattingContextForInFlowBlockLevelChild(
    const NGConstraintSpace& space,
    const ComputedStyle& style) {
  // TODO(layout-dev): This doesn't capture a few cases which can't be computed
  // directly from style yet:
  //  - The child is a <fieldset>.
  //  - "column-span: all" is set on the child (requires knowledge that we are
  //    in a multi-col formatting context).
  //    (https://drafts.csswg.org/css-multicol-1/#valdef-column-span-all)

  if (style.specifiesColumns() || style.containsPaint() ||
      style.containsLayout())
    return true;

  if (!style.isOverflowVisible())
    return true;

  EDisplay display = style.display();
  if (display == EDisplay::Grid || display == EDisplay::Flex ||
      display == EDisplay::WebkitBox)
    return true;

  if (space.WritingMode() != FromPlatformWritingMode(style.getWritingMode()))
    return true;

  return false;
}

}  // namespace

NGBlockLayoutAlgorithm::NGBlockLayoutAlgorithm(
    PassRefPtr<const ComputedStyle> style,
    NGBlockNode* first_child,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kBlockLayoutAlgorithm),
      layout_state_(kStateInit),
      compute_minmax_state_(kStateInit),
      style_(style),
      first_child_(first_child),
      constraint_space_(constraint_space),
      break_token_(break_token),
      is_fragment_margin_strut_block_start_updated_(false) {
  DCHECK(style_);
}

NGLayoutAlgorithm::MinAndMaxState
NGBlockLayoutAlgorithm::ComputeMinAndMaxContentSizes(
    MinAndMaxContentSizes* sizes) {
  switch (compute_minmax_state_) {
    case kStateInit:
      pending_minmax_sizes_.min_content = pending_minmax_sizes_.max_content =
          LayoutUnit();
      // Size-contained elements don't consider their contents for intrinsic
      // sizing.
      if (Style().containsSize())
        return kSuccess;
      current_minmax_child_ = first_child_;
      compute_minmax_state_ = kStateChildLayout;
    case kStateChildLayout:
      // TODO: handle floats & orthogonal children
      if (current_minmax_child_) {
        Optional<MinAndMaxContentSizes> child_minmax;
        if (NeedMinAndMaxContentSizesForContentContribution(
                *current_minmax_child_->Style())) {
          child_minmax = MinAndMaxContentSizes();
          if (!current_minmax_child_->ComputeMinAndMaxContentSizes(
                  &*child_minmax))
            return kPending;
        }
        MinAndMaxContentSizes child_sizes = ComputeMinAndMaxContentContribution(
            *current_minmax_child_->Style(), child_minmax);
        pending_minmax_sizes_.min_content = std::max(
            pending_minmax_sizes_.min_content, child_sizes.min_content);
        pending_minmax_sizes_.max_content = std::max(
            pending_minmax_sizes_.max_content, child_sizes.max_content);

        current_minmax_child_ = current_minmax_child_->NextSibling();
        if (current_minmax_child_)
          return kPending;
      }

      *sizes = pending_minmax_sizes_;
      sizes->max_content = std::max(sizes->min_content, sizes->max_content);
      compute_minmax_state_ = kStateInit;
      return kSuccess;
    default:
      NOTREACHED();
      return kSuccess;
  };
}

NGLayoutStatus NGBlockLayoutAlgorithm::Layout(
    NGPhysicalFragment* child_fragment,
    NGPhysicalFragment** fragment_out,
    NGLayoutAlgorithm** algorithm_out) {
  switch (layout_state_) {
    case kStateInit: {
      WTF::Optional<MinAndMaxContentSizes> sizes;
      if (NeedMinAndMaxContentSizes(ConstraintSpace(), Style())) {
        sizes = MinAndMaxContentSizes();
        if (ComputeMinAndMaxContentSizes(&*sizes) == kPending)
          return kNotFinished;
      }

      border_and_padding_ =
          ComputeBorders(Style()) + ComputePadding(ConstraintSpace(), Style());

      LayoutUnit inline_size =
          ComputeInlineSizeForFragment(ConstraintSpace(), Style(), sizes);
      LayoutUnit adjusted_inline_size =
          inline_size - border_and_padding_.InlineSum();
      // TODO(layout-ng): For quirks mode, should we pass blockSize instead of
      // -1?
      LayoutUnit block_size = ComputeBlockSizeForFragment(
          ConstraintSpace(), Style(), NGSizeIndefinite);
      LayoutUnit adjusted_block_size(block_size);
      // Our calculated block-axis size may be indefinite at this point.
      // If so, just leave the size as NGSizeIndefinite instead of subtracting
      // borders and padding.
      if (adjusted_block_size != NGSizeIndefinite)
        adjusted_block_size -= border_and_padding_.BlockSum();

      space_builder_ = new NGConstraintSpaceBuilder(constraint_space_);
      if (Style().specifiesColumns()) {
        space_builder_->SetFragmentationType(kFragmentColumn);
        adjusted_inline_size =
            ResolveUsedColumnInlineSize(adjusted_inline_size, Style());
      }
      space_builder_->SetAvailableSize(
          NGLogicalSize(adjusted_inline_size, adjusted_block_size));
      space_builder_->SetPercentageResolutionSize(
          NGLogicalSize(adjusted_inline_size, adjusted_block_size));

      content_size_ = border_and_padding_.block_start;

      builder_ = new NGFragmentBuilder(NGPhysicalFragment::kFragmentBox);
      builder_->SetDirection(constraint_space_->Direction());
      builder_->SetWritingMode(constraint_space_->WritingMode());
      builder_->SetInlineSize(inline_size).SetBlockSize(block_size);

      current_child_ = first_child_;
      layout_state_ = kStatePrepareForChildLayout;
      return kNotFinished;
    }
    case kStatePrepareForChildLayout: {
      if (current_child_) {
        EPosition position = current_child_->Style()->position();
        if ((position == AbsolutePosition || position == FixedPosition)) {
          builder_->AddOutOfFlowChildCandidate(current_child_,
                                               GetChildSpaceOffset());
          current_child_ = current_child_->NextSibling();
          return kNotFinished;
        }
        space_for_current_child_ = CreateConstraintSpaceForCurrentChild();
        *algorithm_out = NGLayoutInputNode::AlgorithmForInputNode(
            current_child_, space_for_current_child_);
        layout_state_ = kStateChildLayout;
        return kChildAlgorithmRequired;
      }

      // Prepare for kStateOutOfFlowLayout
      content_size_ += border_and_padding_.block_end;

      // Recompute the block-axis size now that we know our content size.
      LayoutUnit block_size = ComputeBlockSizeForFragment(
          ConstraintSpace(), Style(), content_size_);
      builder_->SetBlockSize(block_size);

      // Out of flow setup.
      out_of_flow_layout_ =
          new NGOutOfFlowLayoutPart(&Style(), builder_->Size());
      builder_->GetAndClearOutOfFlowDescendantCandidates(
          &out_of_flow_candidates_, &out_of_flow_candidate_positions_);
      out_of_flow_candidate_positions_index_ = 0;
      current_child_ = nullptr;
      layout_state_ = kStateOutOfFlowLayout;
      return kNotFinished;
    }
    case kStateChildLayout: {
      DCHECK(current_child_);
      DCHECK(child_fragment);

      // TODO(layout_ng): Seems like a giant hack to call this here.
      current_child_->UpdateLayoutBox(toNGPhysicalBoxFragment(child_fragment),
                                      space_for_current_child_);

      FinishCurrentChildLayout(new NGBoxFragment(
          ConstraintSpace().WritingMode(), ConstraintSpace().Direction(),
          toNGPhysicalBoxFragment(child_fragment)));
      current_child_ = current_child_->NextSibling();
      layout_state_ = kStatePrepareForChildLayout;
      return kNotFinished;
    }
    case kStateOutOfFlowLayout:
      if (LayoutOutOfFlowChild())
        layout_state_ = kStateFinalize;
      return kNotFinished;
    case kStateFinalize: {
      builder_->SetInlineOverflow(max_inline_size_)
          .SetBlockOverflow(content_size_);
      *fragment_out = builder_->ToBoxFragment();
      layout_state_ = kStateInit;
      return kNewFragment;
    }
  };
  NOTREACHED();
  *fragment_out = nullptr;
  return kNewFragment;
}

void NGBlockLayoutAlgorithm::FinishCurrentChildLayout(NGFragment* fragment) {
  NGBoxStrut child_margins = ComputeMargins(
      *space_for_current_child_, CurrentChildStyle(),
      constraint_space_->WritingMode(), constraint_space_->Direction());

  NGLogicalOffset fragment_offset;
  if (CurrentChildStyle().isFloating()) {
    fragment_offset = PositionFloatFragment(*fragment, child_margins);
  } else {
    ApplyAutoMargins(*space_for_current_child_, CurrentChildStyle(), *fragment,
                     &child_margins);
    fragment_offset = PositionFragment(*fragment, child_margins);
  }
  builder_->AddChild(fragment, fragment_offset);
}

bool NGBlockLayoutAlgorithm::LayoutOutOfFlowChild() {
  if (!current_child_) {
    if (out_of_flow_candidates_.isEmpty()) {
      out_of_flow_layout_ = nullptr;
      out_of_flow_candidate_positions_.clear();
      return true;
    }
    current_child_ = out_of_flow_candidates_.first();
    out_of_flow_candidates_.removeFirst();
    NGStaticPosition position = out_of_flow_candidate_positions_
        [out_of_flow_candidate_positions_index_++];

    if (!out_of_flow_layout_->StartLayout(current_child_, position)) {
      builder_->AddOutOfFlowDescendant(current_child_, position);
      current_child_ = nullptr;
      return false;
    }
  }
  NGFragment* fragment;
  NGLogicalOffset offset;
  if (out_of_flow_layout_->Layout(&fragment, &offset) == kNewFragment) {
    // TODO(atotic) Need to adjust size of overflow rect per spec.
    builder_->AddChild(fragment, offset);
    current_child_ = nullptr;
  }
  return false;
}

NGBoxStrut NGBlockLayoutAlgorithm::CollapseMargins(
    const NGBoxStrut& margins,
    const NGBoxFragment& fragment) {
  bool is_zero_height_box = !fragment.BlockSize() && margins.IsEmpty() &&
                            fragment.MarginStrut().IsEmpty();
  // Create the current child's margin strut from its children's margin strut or
  // use margin strut from the the last non-empty child.
  NGMarginStrut curr_margin_strut =
      is_zero_height_box ? prev_child_margin_strut_ : fragment.MarginStrut();

  // Calculate borders and padding for the current child.
  NGBoxStrut border_and_padding =
      ComputeBorders(CurrentChildStyle()) +
      ComputePadding(ConstraintSpace(), CurrentChildStyle());

  // Collapse BLOCK-START margins if there is no padding or border between
  // parent (current child) and its first in-flow child.
  if (border_and_padding.block_start) {
    curr_margin_strut.SetMarginBlockStart(margins.block_start);
  } else {
    curr_margin_strut.AppendMarginBlockStart(margins.block_start);
  }

  // Collapse BLOCK-END margins if
  // 1) there is no padding or border between parent (current child) and its
  //    first/last in-flow child
  // 2) parent's logical height is auto.
  if (CurrentChildStyle().logicalHeight().isAuto() &&
      !border_and_padding.block_end) {
    curr_margin_strut.AppendMarginBlockEnd(margins.block_end);
  } else {
    curr_margin_strut.SetMarginBlockEnd(margins.block_end);
  }

  NGBoxStrut result_margins;
  // Margins of the newly established formatting context do not participate
  // in Collapsing Margins:
  // - Compute margins block start for adjoining blocks *including* 1st block.
  // - Compute margins block end for the last block.
  // - Do not set the computed margins to the parent fragment.
  if (constraint_space_->IsNewFormattingContext()) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
    bool is_last_child = !current_child_->NextSibling();
    if (is_last_child)
      result_margins.block_end = curr_margin_strut.BlockEndSum();
    return result_margins;
  }

  // Zero-height boxes are ignored and do not participate in margin collapsing.
  if (is_zero_height_box)
    return result_margins;

  // Compute the margin block start for adjoining blocks *excluding* 1st block
  if (is_fragment_margin_strut_block_start_updated_) {
    result_margins.block_start = ComputeCollapsedMarginBlockStart(
        prev_child_margin_strut_, curr_margin_strut);
  }

  // Update the parent fragment's margin strut
  UpdateMarginStrut(curr_margin_strut);

  prev_child_margin_strut_ = curr_margin_strut;
  return result_margins;
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionFragment(
    const NGFragment& fragment,
    const NGBoxStrut& child_margins) {
  const NGBoxStrut collapsed_margins =
      CollapseMargins(child_margins, toNGBoxFragment(fragment));

  AdjustToClearance(ConstraintSpace(), CurrentChildStyle(), &content_size_);

  LayoutUnit inline_offset =
      border_and_padding_.inline_start + child_margins.inline_start;
  LayoutUnit block_offset = content_size_ + collapsed_margins.block_start;

  content_size_ += fragment.BlockSize() + collapsed_margins.BlockSum();
  max_inline_size_ = std::max(
      max_inline_size_, fragment.InlineSize() + child_margins.InlineSum() +
                            border_and_padding_.InlineSum());
  return NGLogicalOffset(inline_offset, block_offset);
}

NGLogicalOffset NGBlockLayoutAlgorithm::PositionFloatFragment(
    const NGFragment& fragment,
    const NGBoxStrut& margins) {
  // TODO(glebl@chromium.org): Support the top edge alignment rule.
  // Find a layout opportunity that will fit our float.

  // Update offset if there is a clearance.
  NGLogicalOffset offset = space_for_current_child_->Offset();
  AdjustToClearance(ConstraintSpace(), CurrentChildStyle(),
                    &offset.block_offset);
  space_for_current_child_->SetOffset(offset);

  const NGLayoutOpportunity opportunity = FindLayoutOpportunityForFragment(
      space_for_current_child_, fragment, margins);
  DCHECK(!opportunity.IsEmpty()) << "Opportunity is empty but it shouldn't be";

  NGExclusion::Type exclusion_type = NGExclusion::kFloatLeft;
  // Calculate the float offset if needed.
  LayoutUnit float_offset;
  if (CurrentChildStyle().floating() == EFloat::kRight) {
    float_offset = opportunity.size.inline_size - fragment.InlineSize();
    exclusion_type = NGExclusion::kFloatRight;
  }

  // Add the float as an exclusion.
  const NGExclusion exclusion = CreateExclusion(
      fragment, opportunity, float_offset, margins, exclusion_type);
  constraint_space_->AddExclusion(exclusion);

  return CalculateLogicalOffsetForOpportunity(opportunity, float_offset,
                                              margins);
}

void NGBlockLayoutAlgorithm::UpdateMarginStrut(const NGMarginStrut& from) {
  if (!is_fragment_margin_strut_block_start_updated_) {
    builder_->SetMarginStrutBlockStart(from);
    is_fragment_margin_strut_block_start_updated_ = true;
  }
  builder_->SetMarginStrutBlockEnd(from);
}

NGConstraintSpace*
NGBlockLayoutAlgorithm::CreateConstraintSpaceForCurrentChild() const {
  // TODO(layout-ng): Orthogonal children should also shrink to fit (in *their*
  // inline axis)
  // We have to keep this commented out for now until we correctly compute
  // min/max content sizes in Layout().
  bool shrink_to_fit = CurrentChildStyle().display() == EDisplay::InlineBlock ||
                       CurrentChildStyle().isFloating();
  DCHECK(current_child_);
  space_builder_
      ->SetIsNewFormattingContext(
          IsNewFormattingContextForInFlowBlockLevelChild(ConstraintSpace(),
                                                         CurrentChildStyle()))
      .SetIsShrinkToFit(shrink_to_fit)
      .SetWritingMode(
          FromPlatformWritingMode(CurrentChildStyle().getWritingMode()))
      .SetTextDirection(CurrentChildStyle().direction());
  NGConstraintSpace* child_space = space_builder_->ToConstraintSpace();

  // TODO(layout-ng): Set offset through the space builder.
  child_space->SetOffset(GetChildSpaceOffset());
  return child_space;
}

DEFINE_TRACE(NGBlockLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(first_child_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
  visitor->trace(builder_);
  visitor->trace(space_builder_);
  visitor->trace(space_for_current_child_);
  visitor->trace(current_child_);
  visitor->trace(current_minmax_child_);
  visitor->trace(out_of_flow_layout_);
  visitor->trace(out_of_flow_candidates_);
}

}  // namespace blink
