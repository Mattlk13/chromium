// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/import_data_collection_view_controller.h"

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/card_multiline_item.h"
#import "ios/chrome/browser/ui/settings/cells/import_data_multiline_detail_cell.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Collections/src/MaterialCollections.h"
#include "ui/base/l10n/l10n_util_mac.h"

// The accessibility identifier of the Import Data cell.
NSString* const kImportDataImportCellId = @"kImportDataImportCellId";
// The accessibility identifier of the Keep Data Separate cell.
NSString* const kImportDataKeepSeparateCellId =
    @"kImportDataKeepSeparateCellId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDisclaimer = kSectionIdentifierEnumZero,
  SectionIdentifierOptions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooter = kItemTypeEnumZero,
  ItemTypeOptionImportData,
  ItemTypeOptionKeepDataSeparate,
};

}  // namespace

@implementation ImportDataCollectionViewController {
  base::WeakNSProtocol<id<ImportDataControllerDelegate>> _delegate;
  base::scoped_nsobject<NSString> _fromEmail;
  base::scoped_nsobject<NSString> _toEmail;
  BOOL _isSignedIn;
  ShouldClearData _shouldClearData;
  base::scoped_nsobject<CollectionViewDetailItem> _importDataItem;
  base::scoped_nsobject<CollectionViewDetailItem> _keepDataSeparateItem;
}

#pragma mark Initialization

- (instancetype)initWithDelegate:(id<ImportDataControllerDelegate>)delegate
                       fromEmail:(NSString*)fromEmail
                         toEmail:(NSString*)toEmail
                      isSignedIn:(BOOL)isSignedIn {
  DCHECK(fromEmail);
  DCHECK(toEmail);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    _delegate.reset(delegate);
    _fromEmail.reset([fromEmail copy]);
    _toEmail.reset([toEmail copy]);
    _isSignedIn = isSignedIn;
    _shouldClearData = isSignedIn ? SHOULD_CLEAR_DATA_CLEAR_DATA
                                  : SHOULD_CLEAR_DATA_MERGE_DATA;
    [self setShouldHideDoneButton:YES];
    self.navigationItem.rightBarButtonItem = [[[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON)
                style:UIBarButtonItemStyleDone
               target:self
               action:@selector(didTapContinue)] autorelease];
    [self loadModel];
  }
  return self;
}

#pragma mark SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierDisclaimer];
  [model addItem:[self descriptionItem]
      toSectionWithIdentifier:SectionIdentifierDisclaimer];

  [model addSectionWithIdentifier:SectionIdentifierOptions];
  _importDataItem.reset([[self importDataItem] retain]);
  _keepDataSeparateItem.reset([[self keepDataSeparateItem] retain]);
  if (_isSignedIn) {
    [model addItem:_keepDataSeparateItem
        toSectionWithIdentifier:SectionIdentifierOptions];
    [model addItem:_importDataItem
        toSectionWithIdentifier:SectionIdentifierOptions];
  } else {
    [model addItem:_importDataItem
        toSectionWithIdentifier:SectionIdentifierOptions];
    [model addItem:_keepDataSeparateItem
        toSectionWithIdentifier:SectionIdentifierOptions];
  }
}

#pragma mark Items

- (CollectionViewItem*)descriptionItem {
  CardMultilineItem* item =
      [[[CardMultilineItem alloc] initWithType:ItemTypeFooter] autorelease];
  item.text = l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_HEADER,
                                      base::SysNSStringToUTF16(_fromEmail));
  return item;
}

- (CollectionViewDetailItem*)importDataItem {
  CollectionViewDetailItem* item = [[[CollectionViewDetailItem alloc]
      initWithType:ItemTypeOptionImportData] autorelease];
  item.cellClass = [ImportDataMultilineDetailCell class];
  item.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_TITLE);
  item.detailText =
      l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_SUBTITLE,
                              base::SysNSStringToUTF16(_toEmail));
  item.accessoryType = _isSignedIn ? MDCCollectionViewCellAccessoryNone
                                   : MDCCollectionViewCellAccessoryCheckmark;
  item.accessibilityIdentifier = kImportDataImportCellId;
  return item;
}

- (CollectionViewDetailItem*)keepDataSeparateItem {
  CollectionViewDetailItem* item = [[[CollectionViewDetailItem alloc]
      initWithType:ItemTypeOptionKeepDataSeparate] autorelease];
  item.cellClass = [ImportDataMultilineDetailCell class];
  item.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_TITLE);
  if (_isSignedIn) {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SUBTITLE_SWITCH,
        base::SysNSStringToUTF16(_fromEmail));
  } else {
    item.detailText = l10n_util::GetNSString(
        IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SUBTITLE_SIGNIN);
  }
  item.accessoryType = _isSignedIn ? MDCCollectionViewCellAccessoryCheckmark
                                   : MDCCollectionViewCellAccessoryNone;
  item.accessibilityIdentifier = kImportDataKeepSeparateCellId;
  return item;
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  CGFloat cardWidth = CGRectGetWidth(collectionView.bounds) -
                      2 * MDCCollectionViewCellStyleCardSectionInset;
  return
      [MDCCollectionViewCell cr_preferredHeightForWidth:cardWidth forItem:item];
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];

  if (sectionIdentifier == SectionIdentifierOptions) {
    // Store the user choice.
    NSInteger itemType =
        [self.collectionViewModel itemTypeForIndexPath:indexPath];
    _shouldClearData = (itemType == ItemTypeOptionImportData)
                           ? SHOULD_CLEAR_DATA_MERGE_DATA
                           : SHOULD_CLEAR_DATA_CLEAR_DATA;
    [self updateUI];
  }
}

#pragma mark Private methods

// Updates the UI based on the value of |_shouldClearData|.
- (void)updateUI {
  BOOL importDataSelected = _shouldClearData == SHOULD_CLEAR_DATA_MERGE_DATA;
  _importDataItem.get().accessoryType =
      importDataSelected ? MDCCollectionViewCellAccessoryCheckmark
                         : MDCCollectionViewCellAccessoryNone;
  _keepDataSeparateItem.get().accessoryType =
      importDataSelected ? MDCCollectionViewCellAccessoryNone
                         : MDCCollectionViewCellAccessoryCheckmark;
  [self reconfigureCellsForItems:@[ _importDataItem, _keepDataSeparateItem ]
         inSectionWithIdentifier:SectionIdentifierOptions];
}

- (void)didTapContinue {
  [_delegate didChooseClearDataPolicy:self shouldClearData:_shouldClearData];
}

@end
