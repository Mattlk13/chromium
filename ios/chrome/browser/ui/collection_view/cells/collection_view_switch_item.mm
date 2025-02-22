// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;
}  // namespace

@implementation CollectionViewSwitchItem

@synthesize text = _text;
@synthesize on = _on;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewSwitchCell class];
    self.enabled = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(CollectionViewSwitchCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.switchView.enabled = self.isEnabled;

  // Force disabled cells to be drawn in the "off" state, but do not change the
  // value of the |on| property.
  cell.switchView.on = self.isOn && self.isEnabled;
  cell.textLabel.textColor =
      [CollectionViewSwitchCell defaultTextColorForState:cell.switchView.state];
}

@end

@implementation CollectionViewSwitchCell

@synthesize textLabel = _textLabel;
@synthesize switchView = _switchView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_textLabel];

    _textLabel.font =
        [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:14];
    _textLabel.textColor = [[MDCPalette greyPalette] tint900];
    _textLabel.numberOfLines = 0;

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    _switchView.onTintColor = [[MDCPalette cr_bluePalette] tint500];
    [self.contentView addSubview:_switchView];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_switchView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_switchView.leadingAnchor
                                   constant:-kHorizontalPadding],
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kVerticalPadding],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_switchView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

+ (UIColor*)defaultTextColorForState:(UIControlState)state {
  MDCPalette* grey = [MDCPalette greyPalette];
  return (state & UIControlStateDisabled) ? grey.tint500 : grey.tint900;
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  _textLabel.preferredMaxLayoutWidth = CGRectGetWidth(self.contentView.frame) -
                                       CGRectGetWidth(_switchView.frame) -
                                       3 * kHorizontalPadding;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];

  [_switchView removeTarget:nil
                     action:nil
           forControlEvents:[_switchView allControlEvents]];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  // Center the activation point over the switch, so that double-tapping toggles
  // the switch.
  CGRect switchFrame =
      UIAccessibilityConvertFrameToScreenCoordinates(_switchView.frame, self);
  return CGPointMake(CGRectGetMidX(switchFrame), CGRectGetMidY(switchFrame));
}

- (NSString*)accessibilityHint {
  return _switchView.accessibilityHint;
}

- (NSString*)accessibilityLabel {
  return _textLabel.text;
}

- (NSString*)accessibilityValue {
  return _switchView.accessibilityValue;
}

@end
