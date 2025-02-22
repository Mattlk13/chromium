// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_entry_inserter.h"

#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#include "ios/chrome/browser/ui/history/history_util.h"
#include "url/gurl.h"

@interface HistoryEntryInserter () {
  // Delegate for the HistoryEntryInserter.
  base::WeakNSProtocol<id<HistoryEntryInserterDelegate>> _delegate;
  // CollectionViewModel in which to insert history entries.
  base::scoped_nsobject<CollectionViewModel> _collectionViewModel;
  // The index of the first section to contain history entries.
  NSInteger _firstSectionIndex;
  // Number of assigned section identifiers.
  NSInteger _sectionIdentifierCount;
  // Sorted set of dates that have history entries.
  base::scoped_nsobject<NSMutableOrderedSet> _dates;
  // Mapping from dates to section identifiers.
  base::scoped_nsobject<NSMutableDictionary> _sectionIdentifiers;
}

@end

@implementation HistoryEntryInserter

- (instancetype)initWithModel:(CollectionViewModel*)collectionViewModel {
  if ((self = [super init])) {
    _collectionViewModel.reset([collectionViewModel retain]);
    _firstSectionIndex = [collectionViewModel numberOfSections];
    _dates.reset([[NSMutableOrderedSet alloc] init]);
    _sectionIdentifiers.reset([[NSMutableDictionary dictionary] retain]);
  }
  return self;
}

- (id<HistoryEntryInserterDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<HistoryEntryInserterDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)insertHistoryEntryItem:(HistoryEntryItem*)item {
  NSInteger sectionIdentifier =
      [self sectionIdentifierForTimestamp:item.timestamp];

  NSComparator objectComparator = ^(id obj1, id obj2) {
    HistoryEntryItem* firstObject =
        base::mac::ObjCCastStrict<HistoryEntryItem>(obj1);
    HistoryEntryItem* secondObject =
        base::mac::ObjCCastStrict<HistoryEntryItem>(obj2);
    if ([firstObject isEqualToHistoryEntryItem:secondObject])
      return NSOrderedSame;

    // History entries are ordered from most to least recent.
    if (firstObject.timestamp > secondObject.timestamp)
      return NSOrderedAscending;
    if (firstObject.timestamp < secondObject.timestamp)
      return NSOrderedDescending;
    return firstObject.URL < secondObject.URL ? NSOrderedAscending
                                              : NSOrderedDescending;
  };

  NSArray* items =
      [_collectionViewModel itemsInSectionWithIdentifier:sectionIdentifier];
  NSRange range = NSMakeRange(0, [items count]);
  // If the object is not already in the section, insert it.
  if ([items indexOfObject:item
             inSortedRange:range
                   options:NSBinarySearchingFirstEqual
           usingComparator:objectComparator] == NSNotFound) {
    // Insert the object at the appropriate index to keep the section sorted.
    NSUInteger index = [items indexOfObject:item
                              inSortedRange:range
                                    options:NSBinarySearchingInsertionIndex
                            usingComparator:objectComparator];
    [_collectionViewModel insertItem:item
             inSectionWithIdentifier:sectionIdentifier
                             atIndex:index];
    NSIndexPath* indexPath = [NSIndexPath
        indexPathForItem:index
               inSection:[_collectionViewModel
                             sectionForSectionIdentifier:sectionIdentifier]];
    [self.delegate historyEntryInserter:self
               didInsertItemAtIndexPath:indexPath];
  }
}

- (NSUInteger)sectionIdentifierForTimestamp:(base::Time)timestamp {
  base::TimeDelta timeDelta =
      timestamp.LocalMidnight() - base::Time::UnixEpoch();
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:timeDelta.InSeconds()];

  NSInteger sectionIdentifier =
      [[_sectionIdentifiers objectForKey:date] integerValue];
  // If there is a section identifier for the date, return it.
  if (sectionIdentifier) {
    return sectionIdentifier;
  }

  // Get the next section identifier, and add a section for date.
  sectionIdentifier =
      kSectionIdentifierEnumZero + _firstSectionIndex + _sectionIdentifierCount;
  ++_sectionIdentifierCount;
  [_sectionIdentifiers setObject:@(sectionIdentifier) forKey:date];

  NSComparator comparator = ^(id obj1, id obj2) {
    // Dates are ordered from most to least recent.
    return [obj2 compare:obj1];
  };
  NSUInteger index = [_dates indexOfObject:date
                             inSortedRange:NSMakeRange(0, [_dates count])
                                   options:NSBinarySearchingInsertionIndex
                           usingComparator:comparator];
  [_dates insertObject:date atIndex:index];
  NSInteger insertionIndex = _firstSectionIndex + index;
  CollectionViewTextItem* header = [[[CollectionViewTextItem alloc]
      initWithType:kItemTypeEnumZero] autorelease];
  header.text =
      base::SysUTF16ToNSString(history::GetRelativeDateLocalized(timestamp));
  [_collectionViewModel insertSectionWithIdentifier:sectionIdentifier
                                            atIndex:insertionIndex];
  [_collectionViewModel setHeader:header
         forSectionWithIdentifier:sectionIdentifier];
  [self.delegate historyEntryInserter:self
              didInsertSectionAtIndex:insertionIndex];
  return sectionIdentifier;
}

- (void)removeSection:(NSInteger)sectionIndex {
  NSUInteger sectionIdentifier =
      [_collectionViewModel sectionIdentifierForSection:sectionIndex];

  // Sections should not be removed unless there are no items in that section.
  DCHECK(![[_collectionViewModel itemsInSectionWithIdentifier:sectionIdentifier]
      count]);
  [_collectionViewModel removeSectionWithIdentifier:sectionIdentifier];

  NSEnumerator* dateEnumerator = [_sectionIdentifiers keyEnumerator];
  NSDate* date = nil;
  while ((date = [dateEnumerator nextObject])) {
    if ([[_sectionIdentifiers objectForKey:date] unsignedIntegerValue] ==
        sectionIdentifier) {
      [_sectionIdentifiers removeObjectForKey:date];
      [_dates removeObject:date];
      break;
    }
  }
  [self.delegate historyEntryInserter:self
              didRemoveSectionAtIndex:sectionIndex];
}

@end
