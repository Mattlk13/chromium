// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('metrics reporting', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  var testBrowserProxy;

  /** @type {SettingsPrivacyPageElement} */
  var page;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-privacy-page');
  });

  teardown(function() { page.remove(); });

  test('changes to whether metrics reporting is enabled/managed', function() {
    return testBrowserProxy.whenCalled('getMetricsReporting').then(function() {
      Polymer.dom.flush();

      var checkbox = page.$.metricsReportingCheckbox;
      assertEquals(testBrowserProxy.metricsReporting.enabled, checkbox.checked);
      var indicatorVisible = !!page.$$('#indicator');
      assertEquals(testBrowserProxy.metricsReporting.managed, indicatorVisible);

      var changedMetrics = {
        enabled: !testBrowserProxy.metricsReporting.enabled,
        managed: !testBrowserProxy.metricsReporting.managed,
      };
      cr.webUIListenerCallback('metrics-reporting-change', changedMetrics);
      Polymer.dom.flush();

      assertEquals(changedMetrics.enabled, checkbox.checked);
      indicatorVisible = !!page.$$('#indicator');
      assertEquals(changedMetrics.managed, indicatorVisible);

      var toggled = !changedMetrics.enabled;

      MockInteractions.tap(checkbox);
      return testBrowserProxy.whenCalled('setMetricsReportingEnabled', toggled);
    });
  });

  test('metrics reporting restart button', function() {
    return testBrowserProxy.whenCalled('getMetricsReporting').then(function() {
      Polymer.dom.flush();

      // Restart button should be hidden by default (in any state).
      assertFalse(!!page.$$('#restart'));

      // Simulate toggling via policy.
      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: true,
      });
      Polymer.dom.flush();

      // No restart button should show because the value is managed.
      assertFalse(!!page.$$('#restart'));

      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: true,
        managed: true,
      });
      Polymer.dom.flush();

      // Changes in policy should not show the restart button because the value
      // is still managed.
      assertFalse(!!page.$$('#restart'));

      // Remove the policy and toggle the value.
      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: false,
      });
      Polymer.dom.flush();

      // Now the restart button should be showing.
      assertTrue(!!page.$$('#restart'));

      // Receiving the same values should have no effect.
       cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: false,
      });
      Polymer.dom.flush();
      assertTrue(!!page.$$('#restart'));
    });
  });
});
