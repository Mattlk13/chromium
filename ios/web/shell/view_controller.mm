// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include <stdint.h>

#include <memory>
#include <utility>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/crn_http_protocol_handler.h"
#import "ios/net/empty_nsurlcache.h"
#import "ios/net/request_tracker.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kWebShellBackButtonAccessibilityLabel = @"Back";
NSString* const kWebShellForwardButtonAccessibilityLabel = @"Forward";
NSString* const kWebShellAddressFieldAccessibilityLabel = @"Address field";

using web::NavigationManager;

@interface ViewController ()<CRWWebStateDelegate,
                             CRWWebStateObserver,
                             UITextFieldDelegate,
                             UIToolbarDelegate> {
  web::BrowserState* _browserState;
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
}
@property(nonatomic, assign, readonly) NavigationManager* navigationManager;
@property(nonatomic, readwrite, strong) UITextField* field;
@end

@implementation ViewController

@synthesize field = _field;
@synthesize containerView = _containerView;
@synthesize toolbarView = _toolbarView;

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (void)dealloc {
  net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);
  net::RequestTracker::SetRequestTrackerFactory(nullptr);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  CGRect bounds = self.view.bounds;

  // Set up the toolbar.
  _toolbarView = [[UIToolbar alloc] init];
  _toolbarView.barTintColor =
      [UIColor colorWithRed:0.337 green:0.467 blue:0.988 alpha:1.0];
  _toolbarView.frame = CGRectMake(0, 20, CGRectGetWidth(bounds), 44);
  _toolbarView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleBottomMargin;
  _toolbarView.delegate = self;
  [self.view addSubview:_toolbarView];

  // Set up the container view.
  _containerView = [[UIView alloc] init];
  _containerView.frame =
      CGRectMake(0, 64, CGRectGetWidth(bounds), CGRectGetHeight(bounds) - 64);
  _containerView.backgroundColor = [UIColor lightGrayColor];
  _containerView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_containerView];

  // Set up the toolbar buttons.
  UIButton* back = [UIButton buttonWithType:UIButtonTypeCustom];
  [back setImage:[UIImage imageNamed:@"toolbar_back"]
        forState:UIControlStateNormal];
  [back setFrame:CGRectMake(0, 0, 44, 44)];
  [back setImageEdgeInsets:UIEdgeInsetsMake(5, 5, 4, 4)];
  [back setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [back addTarget:self
                action:@selector(back)
      forControlEvents:UIControlEventTouchUpInside];
  [back setAccessibilityLabel:kWebShellBackButtonAccessibilityLabel];

  UIButton* forward = [UIButton buttonWithType:UIButtonTypeCustom];
  [forward setImage:[UIImage imageNamed:@"toolbar_forward"]
           forState:UIControlStateNormal];
  [forward setFrame:CGRectMake(44, 0, 44, 44)];
  [forward setImageEdgeInsets:UIEdgeInsetsMake(5, 5, 4, 4)];
  [forward setAutoresizingMask:UIViewAutoresizingFlexibleRightMargin];
  [forward addTarget:self
                action:@selector(forward)
      forControlEvents:UIControlEventTouchUpInside];
  [forward setAccessibilityLabel:kWebShellForwardButtonAccessibilityLabel];

  base::scoped_nsobject<UITextField> field([[UITextField alloc]
      initWithFrame:CGRectMake(88, 6, CGRectGetWidth([_toolbarView frame]) - 98,
                               31)]);
  [field setDelegate:self];
  [field setBackground:[[UIImage imageNamed:@"textfield_background"]
                           resizableImageWithCapInsets:UIEdgeInsetsMake(
                                                           12, 12, 12, 12)]];
  [field setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [field setKeyboardType:UIKeyboardTypeWebSearch];
  [field setAutocorrectionType:UITextAutocorrectionTypeNo];
  [field setAccessibilityLabel:kWebShellAddressFieldAccessibilityLabel];
  [field setClearButtonMode:UITextFieldViewModeWhileEditing];
  self.field = field;

  [_toolbarView addSubview:back];
  [_toolbarView addSubview:forward];
  [_toolbarView addSubview:field];

  // Set up the network stack before creating the WebState.
  [self setUpNetworkStack];

  web::WebState::CreateParams webStateCreateParams(_browserState);
  _webState = web::WebState::Create(webStateCreateParams);
  _webState->SetWebUsageEnabled(true);

  _webStateObserver.reset(
      new web::WebStateObserverBridge(_webState.get(), self));
  _webStateDelegate.reset(new web::WebStateDelegateBridge(self));
  _webState->SetDelegate(_webStateDelegate.get());

  UIView* view = _webState->GetView();
  [view setFrame:[_containerView bounds]];
  [_containerView addSubview:view];

  NavigationManager::WebLoadParams params(GURL("https://dev.chromium.org/"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  self.navigationManager->LoadURLWithParams(params);
}

- (NavigationManager*)navigationManager {
  return _webState->GetNavigationManager();
}

- (web::WebState*)webState {
  return _webState.get();
}

- (void)setUpNetworkStack {
  // Disable the default cache.
  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
}

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  if (bar == _toolbarView) {
    return UIBarPositionTopAttached;
  }
  return UIBarPositionAny;
}

- (void)back {
  if (self.navigationManager->CanGoBack()) {
    self.navigationManager->GoBack();
  }
}

- (void)forward {
  if (self.navigationManager->CanGoForward()) {
    self.navigationManager->GoForward();
  }
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  GURL URL = GURL(base::SysNSStringToUTF8([field text]));

  // Do not try to load invalid URLs.
  if (URL.is_valid()) {
    NavigationManager::WebLoadParams params(URL);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    self.navigationManager->LoadURLWithParams(params);
  }

  [field resignFirstResponder];
  [self updateToolbar];
  return YES;
}

- (void)updateToolbar {
  // Do not update the URL if the text field is currently being edited.
  if ([_field isFirstResponder]) {
    return;
  }

  const GURL& visibleURL = _webState->GetVisibleURL();
  [_field setText:base::SysUTF8ToNSString(visibleURL.spec())];
}

// -----------------------------------------------------------------------
#pragma mark Bikeshedding Implementation

// Overridden to allow this view controller to receive motion events by being
// first responder when no other views are.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent*)event {
  if (event.subtype == UIEventSubtypeMotionShake) {
    [self updateToolbarColor];
  }
}

- (void)updateToolbarColor {
  // Cycle through the following set of colors:
  NSArray* colors = @[
    // Vanilla Blue.
    [UIColor colorWithRed:0.337 green:0.467 blue:0.988 alpha:1.0],
    // Vanilla Red.
    [UIColor colorWithRed:0.898 green:0.110 blue:0.137 alpha:1.0],
    // Blue Grey.
    [UIColor colorWithRed:0.376 green:0.490 blue:0.545 alpha:1.0],
    // Brown.
    [UIColor colorWithRed:0.475 green:0.333 blue:0.282 alpha:1.0],
    // Purple.
    [UIColor colorWithRed:0.612 green:0.153 blue:0.690 alpha:1.0],
    // Teal.
    [UIColor colorWithRed:0.000 green:0.737 blue:0.831 alpha:1.0],
    // Deep Orange.
    [UIColor colorWithRed:1.000 green:0.341 blue:0.133 alpha:1.0],
    // Indigo.
    [UIColor colorWithRed:0.247 green:0.318 blue:0.710 alpha:1.0],
    // Vanilla Green.
    [UIColor colorWithRed:0.145 green:0.608 blue:0.141 alpha:1.0],
    // Pinkerton.
    [UIColor colorWithRed:0.914 green:0.118 blue:0.388 alpha:1.0],
  ];

  NSUInteger currentIndex = [colors indexOfObject:_toolbarView.barTintColor];
  if (currentIndex == NSNotFound) {
    currentIndex = 0;
  }
  NSUInteger newIndex = currentIndex + 1;
  if (newIndex >= [colors count]) {
    // TODO(rohitrao): Out of colors!  Consider prompting the user to pick their
    // own color here.  Also consider allowing the user to choose the entire set
    // of colors or allowing the user to choose color randomization.
    newIndex = 0;
  }
  _toolbarView.barTintColor = [colors objectAtIndex:newIndex];
}

// -----------------------------------------------------------------------
// WebStateObserver implementation.

- (void)didStartProvisionalNavigationForURL:(const GURL&)URL {
  [self updateToolbar];
}

- (void)didCommitNavigationWithDetails:
    (const web::LoadCommittedDetails&)details {
  [self updateToolbar];
}

- (void)webStateDidLoadPage:(web::WebState*)webState withSuccess:(BOOL)success {
  DCHECK_EQ(_webState.get(), webState);
  [self updateToolbar];
}

// -----------------------------------------------------------------------
// WebStateDelegate implementation.

- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  GURL link = params.link_url;
  if (!link.is_valid()) {
    return NO;
  }

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:params.menu_title
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.popoverPresentationController.sourceView = params.view;
  alert.popoverPresentationController.sourceRect =
      CGRectMake(params.location.x, params.location.y, 1.0, 1.0);

  void (^handler)(UIAlertAction*) = ^(UIAlertAction*) {
    NSDictionary* item = @{
      static_cast<NSString*>(kUTTypeURL) : net::NSURLWithGURL(link),
      static_cast<NSString*>(kUTTypeUTF8PlainText) : [base::SysUTF8ToNSString(
          link.spec()) dataUsingEncoding:NSUTF8StringEncoding],
    };
    [[UIPasteboard generalPasteboard] setItems:@[ item ]];
  };
  [alert addAction:[UIAlertAction actionWithTitle:@"Copy Link"
                                            style:UIAlertActionStyleDefault
                                          handler:handler]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  [self presentViewController:alert animated:YES completion:nil];

  return YES;
}

@end
