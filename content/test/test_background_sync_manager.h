// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_BACKGROUND_SYNC_MANAGER_H_
#define CONTENT_TEST_TEST_BACKGROUND_SYNC_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

struct BackgroundSyncParameters;
class DevToolsBackgroundServicesContextImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerVersion;

// A BackgroundSyncManager for use in unit tests. This class provides control
// over internal behavior and state, to allow tests to simulate various
// scenarios.  Examples include (but are not limited to):
//  - Delaying and corrupting the backend storage.
//  - Controlling the firing of service worker onsync events.
//  - Setting the network state
class TestBackgroundSyncManager : public BackgroundSyncManager {
 public:
  using DispatchSyncCallback =
      base::RepeatingCallback<void(scoped_refptr<ServiceWorkerVersion>,
                                   ServiceWorkerVersion::StatusCallback)>;

  TestBackgroundSyncManager(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context);
  ~TestBackgroundSyncManager() override;

  // Force a call to the internal Init() method.
  void DoInit();

  // Resume any delayed backend operation.
  void ResumeBackendOperation();

  // Clear the delayed task if it exists. Delayed tasks are tasks posted with a
  // delay to trigger the BackgroundSyncManager to try firing its sync events
  // again.
  void ClearDelayedTask();

  // Determine whether backend operations succeed or fail due to
  // corruption.
  void set_corrupt_backend(bool corrupt_backend) {
    corrupt_backend_ = corrupt_backend;
  }

  // Delay backend operations until explicitly resumed by calling
  // ResumeBackendOperation().
  void set_delay_backend(bool delay_backend) { delay_backend_ = delay_backend; }

  // Set a callback for when the sync event is dispatched, so tests can observe
  // when the event happens.
  void set_dispatch_sync_callback(const DispatchSyncCallback& callback) {
    dispatch_sync_callback_ = callback;
  }

  // Set a callback for when the periodicSync event is dispatched, so tests can
  // observe it.
  void set_dispatch_periodic_sync_callback(
      const DispatchSyncCallback& callback) {
    dispatch_periodic_sync_callback_ = callback;
  }

  // Sets the response to checks for a main frame for register attempts.
  void set_has_main_frame_provider_host(bool value) {
    has_main_frame_provider_host_ = value;
  }

  // Accessors to internal state
  bool last_chance() const { return last_chance_; }
  const BackgroundSyncParameters* background_sync_parameters() const {
    return parameters_.get();
  }

  void DispatchPeriodicSyncEvent(
      const std::string& tag,
      scoped_refptr<ServiceWorkerVersion> active_version,
      ServiceWorkerVersion::StatusCallback callback) override;

  // Override to allow the test to cache the result.
  base::TimeDelta GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type,
      base::Time last_browser_wakeup_for_periodic_sync) override;

 protected:
  // Override to allow delays to be injected by tests.
  void StoreDataInBackend(
      int64_t sw_registration_id,
      const url::Origin& origin,
      const std::string& key,
      const std::string& data,
      ServiceWorkerStorage::StatusCallback callback) override;

  // Override to allow delays to be injected by tests.
  void GetDataFromBackend(
      const std::string& key,
      ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback callback)
      override;

  // Override to avoid actual dispatching of the event, just call the provided
  // callback instead.
  void DispatchSyncEvent(
      const std::string& tag,
      scoped_refptr<ServiceWorkerVersion> active_version,
      bool last_chance,
      ServiceWorkerVersion::StatusCallback callback) override;

  // Override to avoid actual check for main frame, instead return the value set
  // by tests.
  void HasMainFrameProviderHost(const url::Origin& origin,
                                BoolCallback callback) override;

 private:
  // Callback to resume the StoreDataInBackend operation, after explicit
  // delays injected by tests.
  void StoreDataInBackendContinue(
      int64_t sw_registration_id,
      const url::Origin& origin,
      const std::string& key,
      const std::string& data,
      ServiceWorkerStorage::StatusCallback callback);

  // Callback to resume the GetDataFromBackend operation, after explicit delays
  // injected by tests.
  void GetDataFromBackendContinue(
      const std::string& key,
      ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback callback);

  bool corrupt_backend_ = false;
  bool delay_backend_ = false;
  bool has_main_frame_provider_host_ = true;
  bool last_chance_ = false;
  base::OnceClosure continuation_;
  DispatchSyncCallback dispatch_sync_callback_;
  DispatchSyncCallback dispatch_periodic_sync_callback_;
  base::TimeDelta soonest_one_shot_sync_wakeup_delta_;
  base::TimeDelta soonest_periodic_sync_wakeup_delta_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundSyncManager);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_BACKGROUND_SYNC_MANAGER_H_
