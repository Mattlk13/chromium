// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer elements. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for shared Polymer elements.
 * @constructor
 * @extends {PolymerTest}
 */
function CrElementsBrowserTest() {}

CrElementsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  },

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

function CrElementsLazyRenderTest() {}

CrElementsLazyRenderTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_lazy_render_tests.js'
  ]),
};

TEST_F('CrElementsLazyRenderTest', 'All', function() {
  mocha.run();
});

function CrElementsProfileAvatarSelectorTest() {}

CrElementsProfileAvatarSelectorTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_profile_avatar_selector/' +
      'cr_profile_avatar_selector.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_profile_avatar_selector_tests.js',
  ]),
};

TEST_F('CrElementsProfileAvatarSelectorTest', 'All', function() {
  cr_profile_avatar_selector.registerTests();
  mocha.run();
});

function CrElementsToolbarSearchFieldTest() {}

CrElementsToolbarSearchFieldTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_toolbar_search_field_tests.js',
  ]),
};

TEST_F('CrElementsToolbarSearchFieldTest', 'All', function() {
  cr_toolbar_search_field.registerTests();
  mocha.run();
});

function CrElementsSliderTest() {}

CrElementsSliderTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://resources/cr_elements/cr_slider/cr_slider.html',

  /** @override */
  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_slider_tests.js',
  ]),
};

TEST_F('CrElementsSliderTest', 'All', function() {
  cr_slider.registerTests();
  mocha.run();
});

function CrElementsActionMenuTest() {}

CrElementsActionMenuTest.prototype = {
  __proto__: CrElementsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.html',

  extraLibraries: CrElementsBrowserTest.prototype.extraLibraries.concat([
    'cr_action_menu_test.js',
  ]),
};

TEST_F('CrElementsActionMenuTest', 'All', function() {
  mocha.run();
});
