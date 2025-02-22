// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CollectionViewTextItem

@synthesize accessoryType = _accessoryType;
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize image = _image;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [MDCCollectionViewTextCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(MDCCollectionViewTextCell*)cell {
  [super configureCell:cell];
  cell.accessoryType = self.accessoryType;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.imageView.image = self.image;
  cell.isAccessibilityElement = YES;
  if (self.detailText.length == 0) {
    cell.accessibilityLabel = self.text;
  } else {
    cell.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", self.text, self.detailText];
  }

  // Styling.
  cell.textLabel.font =
      [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:14];
  cell.textLabel.textColor = [[MDCPalette greyPalette] tint900];

  cell.detailTextLabel.font =
      [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:14];
  cell.detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];
}

@end
