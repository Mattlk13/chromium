// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_material_row.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#import "ios/chrome/browser/ui/omnibox/truncating_attributed_label.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

namespace {
const CGFloat kImageDimensionLength = 19.0;
const CGFloat kLeadingPaddingIpad = 164;
const CGFloat kLeadingPaddingIpadCompact = 71;
const CGFloat kAppendButtonTrailingMargin = 4;
}

@interface OmniboxPopupMaterialRow () {
  BOOL _incognito;
  base::mac::ObjCPropertyReleaser _propertyReleaser_OmniboxPopupMaterialRow;
}

// Set the append button normal and highlighted images.
- (void)updateAppendButtonImages;

@end

@implementation OmniboxPopupMaterialRow

@synthesize textTruncatingLabel = _textTruncatingLabel;
@synthesize detailTruncatingLabel = _detailTruncatingLabel;
@synthesize appendButton = _appendButton;
@synthesize physicalWebButton = _physicalWebButton;
@synthesize answerImageView = _answerImageView;
@synthesize imageView = _imageView;
@synthesize rowHeight = _rowHeight;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  return [self initWithIncognito:NO];
}

- (instancetype)initWithIncognito:(BOOL)incognito {
  self = [super initWithStyle:UITableViewCellStyleDefault
              reuseIdentifier:@"OmniboxPopupMaterialRow"];
  if (self) {
    self.isAccessibilityElement = YES;
    self.backgroundColor = [UIColor clearColor];
    _incognito = incognito;
    _propertyReleaser_OmniboxPopupMaterialRow.Init(
        self, [OmniboxPopupMaterialRow class]);

    _textTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    _textTruncatingLabel.userInteractionEnabled = NO;
    [self addSubview:_textTruncatingLabel];

    _detailTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    _detailTruncatingLabel.userInteractionEnabled = NO;
    [self addSubview:_detailTruncatingLabel];

    _appendButton = [[UIButton buttonWithType:UIButtonTypeCustom] retain];
    [_appendButton setContentMode:UIViewContentModeRight];
    [self updateAppendButtonImages];
    // TODO(justincohen): Consider using the UITableViewCell's accessory view.
    // The current implementation is from before using a UITableViewCell.
    [self addSubview:_appendButton];

    _physicalWebButton = [[UIButton buttonWithType:UIButtonTypeCustom] retain];
    [_physicalWebButton setContentMode:UIViewContentModeRight];
    [self updatePhysicalWebImage];
    [self addSubview:_physicalWebButton];

    // Left icon is only displayed on iPad.
    if (IsIPadIdiom()) {
      _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
      _imageView.userInteractionEnabled = NO;
      _imageView.contentMode = UIViewContentModeCenter;

      // TODO(justincohen): Consider using the UITableViewCell's image view.
      // The current implementation is from before using a UITableViewCell.
      [self addSubview:_imageView];
    }

    _answerImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _answerImageView.userInteractionEnabled = NO;
    _answerImageView.contentMode = UIViewContentModeScaleAspectFit;
    [self addSubview:_answerImageView];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self layoutAccessoryViews];
}

- (void)layoutAccessoryViews {
  LayoutRect imageViewLayout = LayoutRectMake(
      IsCompactTablet() ? kLeadingPaddingIpadCompact : kLeadingPaddingIpad,
      CGRectGetWidth(self.bounds),
      floor((_rowHeight - kImageDimensionLength) / 2), kImageDimensionLength,
      kImageDimensionLength);
  _imageView.frame = LayoutRectGetRect(imageViewLayout);

  CGFloat appendButtonDimensionLength = CGRectGetHeight(self.bounds);
  DCHECK_GT(appendButtonDimensionLength, 40);
  LayoutRect rightAccessoryLayout =
      LayoutRectMake(CGRectGetWidth(self.bounds) - appendButtonDimensionLength -
                         kAppendButtonTrailingMargin,
                     CGRectGetWidth(self.bounds),
                     floor((_rowHeight - appendButtonDimensionLength) / 2),
                     appendButtonDimensionLength, appendButtonDimensionLength);
  _appendButton.frame = LayoutRectGetRect(rightAccessoryLayout);
  _physicalWebButton.frame = LayoutRectGetRect(rightAccessoryLayout);
}

- (void)updateLeftImage:(int)imageID {
  _imageView.image = NativeImage(imageID);

  // Adjust the vertical position based on the current size of the row.
  CGRect frame = _imageView.frame;
  frame.origin.y = floor((_rowHeight - kImageDimensionLength) / 2);
  _imageView.frame = frame;
}

- (void)updateHighlightBackground:(BOOL)highlighted {
  // Set the background color to match the color of selected table view cells
  // when their selection style is UITableViewCellSelectionStyleGray.
  if (highlighted) {
    self.backgroundColor = _incognito ? [UIColor colorWithWhite:1 alpha:0.1]
                                      : [UIColor colorWithWhite:0 alpha:0.05];
  } else {
    self.backgroundColor = [UIColor clearColor];
  }
}

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated {
  [super setHighlighted:highlighted animated:animated];
  [self updateHighlightBackground:highlighted];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateHighlightBackground:highlighted];
}

- (void)updateAppendButtonImages {
  int appendResourceID = _incognito
                             ? IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_INCOGNITO
                             : IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
  UIImage* appendImage = NativeReversableImage(appendResourceID, YES);

  [_appendButton setImage:appendImage forState:UIControlStateNormal];
  int appendSelectedResourceID =
      _incognito ? IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_INCOGNITO_HIGHLIGHTED
                 : IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_HIGHLIGHTED;
  UIImage* appendImageSelected =
      NativeReversableImage(appendSelectedResourceID, YES);
  [_appendButton setImage:appendImageSelected
                 forState:UIControlStateHighlighted];
}

- (void)updatePhysicalWebImage {
  UIImage* physicalWebImage = NativeImage(IDR_IOS_OMNIBOX_PHYSICAL_WEB);
  [_physicalWebButton setImage:physicalWebImage forState:UIControlStateNormal];

  UIImage* physicalWebImageSelected =
      NativeImage(IDR_IOS_OMNIBOX_PHYSICAL_WEB_HIGHLIGHTED);
  [_physicalWebButton setImage:physicalWebImageSelected
                      forState:UIControlStateHighlighted];
}

- (NSString*)accessibilityLabel {
  return _textTruncatingLabel.attributedText.string;
}

- (NSString*)accessibilityValue {
  return _detailTruncatingLabel.attributedText.string;
}

@end
