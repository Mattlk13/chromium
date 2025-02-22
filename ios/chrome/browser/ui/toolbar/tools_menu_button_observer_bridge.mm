// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tools_menu_button_observer_bridge.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/reading_list/ios/reading_list_model.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_tools_menu_button.h"

@interface ToolsMenuButtonObserverBridge ()
- (void)updateButtonWithModel:(const ReadingListModel*)model;
- (void)buttonPressed:(UIButton*)sender;
@end

@implementation ToolsMenuButtonObserverBridge {
  base::scoped_nsobject<ToolbarToolsMenuButton> _button;
  ReadingListModel* _model;
  std::unique_ptr<ReadingListModelBridge> _modelBridge;
}

- (instancetype)initWithModel:(ReadingListModel*)readingListModel
                toolbarButton:(ToolbarToolsMenuButton*)button {
  self = [super init];
  if (self) {
    _button.reset([button retain]);
    _model = readingListModel;
    [_button addTarget:self
                  action:@selector(buttonPressed:)
        forControlEvents:UIControlEventTouchUpInside];
    _modelBridge = base::MakeUnique<ReadingListModelBridge>(self, _model);
  }
  return self;
}

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self updateButtonWithModel:model];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  [self updateButtonWithModel:model];
}

- (void)readingListModelBeingDeleted:(const ReadingListModel*)model {
  DCHECK(model == _model);
  _model = nullptr;
}

- (void)updateButtonWithModel:(const ReadingListModel*)model {
  DCHECK(model == _model);
  BOOL readingListContainsUnseenItems = model->GetLocalUnseenFlag();
  [_button setReadingListContainsUnseenItems:readingListContainsUnseenItems];
}

- (void)buttonPressed:(UIButton*)sender {
  if (_model) {
    _model->ResetLocalUnseenFlag();
  }
  [_button setReadingListContainsUnseenItems:NO];
}

@end
