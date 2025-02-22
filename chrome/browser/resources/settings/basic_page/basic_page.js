// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
Polymer({
  is: 'settings-basic-page',

  behaviors: [SettingsPageVisibility, MainPageBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    showAndroidApps: Boolean,

    /**
     * Dictionary defining page visibility.
     * @type {!GuestModePageVisibility}
     */
    pageVisibility: Object,

    advancedToggleExpanded: {
      type: Boolean,
      notify: true,
    },

    /**
     * True if a section is fully expanded to hide other sections beneath it.
     * False otherwise (even while animating a section open/closed).
     * @private {boolean}
     */
    hasExpandedSection_: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the basic page should currently display the reset profile banner.
     * @private {boolean}
     */
    showResetProfileBanner_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showResetProfileBanner');
      },
    },

    /** @private {!settings.Route|undefined} */
    currentRoute_: Object,
  },

  listeners: {
    'subpage-expand': 'onSubpageExpanded_',
  },

  /** @override */
  attached: function() {
    this.currentRoute_ = settings.getCurrentRoute();
  },

  /**
   * Overrides MainPageBehaviorImpl from MainPageBehavior.
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    this.currentRoute_ = newRoute;

    if (settings.Route.ADVANCED.contains(newRoute))
      this.advancedToggleExpanded = true;

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section != oldRoute.section)
        this.hasExpandedSection_ = false;
    } else {
      assert(!this.hasExpandedSection_);
    }

    MainPageBehaviorImpl.currentRouteChanged.call(this, newRoute, oldRoute);
  },

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param {string} query The text to search for.
   * @return {!Promise<!settings.SearchRequest>} A signal indicating that
   *     searching finished.
   */
  searchContents: function(query) {
    var whenSearchDone = settings.getSearchManager().search(
        query, assert(this.$$('#basicPage')));

    if (this.pageVisibility.advancedSettings !== false) {
      assert(whenSearchDone === settings.getSearchManager().search(
          query, assert(this.$$('#advancedPage'))));
    }

    return whenSearchDone;
  },

  /** @private */
  onResetDone_: function() {
    this.showResetProfileBanner_ = false;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowAndroidApps_: function() {
    var visibility = /** @type {boolean|undefined} */ (
        this.get('pageVisibility.androidApps'));
    return this.showAndroidApps && this.showPage(visibility);
  },

  /**
   * Hides everything but the newly expanded subpage.
   * @private
   */
  onSubpageExpanded_: function() {
    this.hasExpandedSection_ = true;
  },

  /**
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean}
   * @private
   */
  showAdvancedToggle_: function(inSearchMode, hasExpandedSection) {
    return !inSearchMode && !hasExpandedSection;
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @return {boolean} Whether to show the basic page, taking into account
   *     both routing and search state.
   * @private
   */
  showBasicPage_: function(currentRoute, inSearchMode, hasExpandedSection) {
    return !hasExpandedSection || settings.Route.BASIC.contains(currentRoute);
  },

  /**
   * @param {!settings.Route} currentRoute
   * @param {boolean} inSearchMode
   * @param {boolean} hasExpandedSection
   * @param {boolean} advancedToggleExpanded
   * @return {boolean} Whether to show the advanced page, taking into account
   *     both routing and search state.
   * @private
   */
  showAdvancedPage_: function(currentRoute, inSearchMode, hasExpandedSection,
                              advancedToggleExpanded) {
    return hasExpandedSection ?
        settings.Route.ADVANCED.contains(currentRoute) :
        advancedToggleExpanded || inSearchMode;
  },

  /**
   * @param {(boolean|undefined)} visibility
   * @return {boolean} True unless visibility is false.
   * @private
   */
  showAdvancedSettings_: function(visibility) {
    return visibility !== false;
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_: function(opened) {
    return opened ? 'settings:arrow-drop-up' : 'cr:arrow-drop-down';
  },
});
