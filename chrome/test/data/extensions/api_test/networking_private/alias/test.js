// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that verifies that apps with only networking API alias permission
// can invoke API methods and listen to API events without encountering
// API access problems.

chrome.test.runTests([
    function onlyAliasBindingsPresent() {
      chrome.test.assertTrue(!!chrome.networking);
      chrome.test.assertTrue(!!chrome.networking.onc);

      chrome.test.assertFalse(!!chrome.networkingPrivate);
      chrome.test.succeed();
    },
    function getProperties() {
      chrome.networking.onc.getProperties(
          'stub_wifi1_guid',
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq('stub_wifi1_guid', result.GUID);
            chrome.test.assertEq(
                chrome.networking.onc.ConnectionStateType.CONNECTED,
                result.ConnectionState);
            chrome.test.assertEq('User', result.Source);
          }));
    },
    function changeConnectionStateAndWaitForNetworksChanged() {
      chrome.test.listenOnce(
          chrome.networking.onc.onNetworksChanged,
          function(networks) {
            chrome.test.assertEq(['stub_wifi1_guid'], networks);
          });
      chrome.networking.onc.startDisconnect(
          'stub_wifi1_guid',
          chrome.test.callbackPass(function() {}));
    },
    function verifyConnectionStateChanged() {
      chrome.networking.onc.getProperties(
          'stub_wifi1_guid',
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq('stub_wifi1_guid', result.GUID);
            chrome.test.assertFalse(
                result.ConnectionState ==
                    chrome.networking.onc.ConnectionStateType.CONNECTED);
          }));
    }
]);
