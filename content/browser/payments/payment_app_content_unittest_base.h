// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTENT_UNITTEST_BASE_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTENT_UNITTEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/payments/payment_app.mojom.h"
#include "content/browser/payments/payment_app_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class EmbeddedWorkerTestHelper;
class PaymentAppContextImpl;
class StoragePartitionImpl;
class TestBrowserThreadBundle;

class PaymentAppContentUnitTestBase : public testing::Test {
 protected:
  PaymentAppContentUnitTestBase();
  ~PaymentAppContentUnitTestBase() override;

  BrowserContext* browser_context();
  PaymentAppManager* CreatePaymentAppManager(const GURL& scope_url,
                                             const GURL& sw_script_url);
  void SetManifest(PaymentAppManager* manager,
                   payments::mojom::PaymentAppManifestPtr manifest,
                   const PaymentAppManager::SetManifestCallback& callback);
  void GetManifest(PaymentAppManager* manager,
                   const PaymentAppManager::GetManifestCallback& callback);
  payments::mojom::PaymentAppManifestPtr CreatePaymentAppManifestForTest(
      const std::string& name);
  void UnregisterServiceWorker(const GURL& scope_url);

 private:
  StoragePartitionImpl* storage_partition();
  PaymentAppContextImpl* payment_app_context();

  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;
  std::vector<payments::mojom::PaymentAppManagerPtr> payment_app_managers_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppContentUnitTestBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTENT_UNITTEST_BASE_H_
