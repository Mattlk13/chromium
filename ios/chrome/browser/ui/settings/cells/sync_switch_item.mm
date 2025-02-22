// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"

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

// Padding used between the switch and text.
const CGFloat kHorizontalSwitchPadding = 10;
}  // namespace

#pragma mark - SyncSwitchItem

@implementation SyncSwitchItem

@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize on = _on;
@synthesize enabled = _enabled;
@synthesize dataType = _dataType;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SyncSwitchCell class];
    self.enabled = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

- (void)configureCell:(SyncSwitchCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.switchView.enabled = self.isEnabled;
  cell.switchView.on = self.isOn;
  cell.textLabel.textColor =
      [SyncSwitchCell defaultTextColorForState:cell.switchView.state];
  if (self.isEnabled) {
    cell.textLabel.textColor = [[MDCPalette greyPalette] tint900];
    cell.switchView.enabled = YES;
    cell.userInteractionEnabled = YES;
  } else {
    cell.textLabel.textColor = [[MDCPalette greyPalette] tint500];
    cell.switchView.enabled = NO;
    cell.userInteractionEnabled = NO;
  }
}

@end

@implementation SyncSwitchCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
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

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_detailTextLabel];

    _detailTextLabel.font =
        [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:14];
    _detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];
    _detailTextLabel.numberOfLines = 0;

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    _switchView.onTintColor = [[MDCPalette cr_bluePalette] tint500];
    [self.contentView addSubview:_switchView];

    [self setConstraints];
  }

  return self;
}

- (void)setConstraints {
  UIView* contentView = self.contentView;

  [NSLayoutConstraint activateConstraints:@[
    [_textLabel.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    [_switchView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPadding],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_switchView.leadingAnchor
                                 constant:-kHorizontalSwitchPadding],
    [_detailTextLabel.trailingAnchor
        constraintEqualToAnchor:_textLabel.trailingAnchor],
    [_textLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                         constant:kVerticalPadding],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kVerticalPadding],
    [_switchView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];
}

+ (UIColor*)defaultTextColorForState:(UIControlState)state {
  MDCPalette* grey = [MDCPalette greyPalette];
  return (state & UIControlStateDisabled) ? grey.tint500 : grey.tint900;
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text and detailText label preferredMaxLayoutWidth when the
  // parent's width
  // changes, for instance on screen rotation.
  CGFloat prefferedMaxLayoutWidth = CGRectGetWidth(self.contentView.frame) -
                                    CGRectGetWidth(_switchView.frame) -
                                    2 * kHorizontalPadding -
                                    kHorizontalSwitchPadding;
  _textLabel.preferredMaxLayoutWidth = prefferedMaxLayoutWidth;
  _detailTextLabel.preferredMaxLayoutWidth = prefferedMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.tag = 0;
  self.textLabel.textColor = [[MDCPalette greyPalette] tint900];
  [self.switchView setEnabled:YES];
  [self setUserInteractionEnabled:YES];

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
  if (_detailTextLabel.text) {
    return [NSString
        stringWithFormat:@"%@, %@", _textLabel.text, _detailTextLabel.text];
  }
  return _textLabel.text;
}

- (NSString*)accessibilityValue {
  return _switchView.accessibilityValue;
}

@end
