// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRY_UPDATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRY_UPDATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class WebApp;
class WebAppRegistrarMutable;
class WebAppSyncBridge;

// A raw registry update data.
struct RegistryUpdateData {
  RegistryUpdateData();
  ~RegistryUpdateData();

  using AppsToCreate = std::vector<std::unique_ptr<WebApp>>;
  AppsToCreate apps_to_create;

  using AppsToDelete = std::vector<AppId>;
  AppsToDelete apps_to_delete;

  using AppsToUpdate = base::flat_set<const WebApp*>;
  AppsToUpdate apps_to_update;

  bool IsEmpty() const;

  DISALLOW_COPY_AND_ASSIGN(RegistryUpdateData);
};

// An explicit writable "view" for the registry. Any write operations must be
// batched as a part of WebAppRegistryUpdate object. Effectively
// WebAppRegistryUpdate is a part of WebAppSyncBridge class.
class WebAppRegistryUpdate {
 public:
  explicit WebAppRegistryUpdate(WebAppRegistrarMutable* mutable_registrar);
  ~WebAppRegistryUpdate();

  // Register a new app.
  void CreateApp(std::unique_ptr<WebApp> web_app);
  // Delete registered app.
  void DeleteApp(const AppId& app_id);
  // Acquire a mutable existing app to set new field values.
  WebApp* UpdateApp(const AppId& app_id);

  const RegistryUpdateData& update_data() const { return *update_data_; }
  std::unique_ptr<RegistryUpdateData> TakeUpdateData();

 private:
  std::unique_ptr<RegistryUpdateData> update_data_;
  WebAppRegistrarMutable* const mutable_registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebAppRegistryUpdate);
};

// A convenience utility class to use RAII for WebAppSyncBridge::BeginUpdate and
// WebAppSyncBridge::CommitUpdate calls.
class ScopedRegistryUpdate {
 public:
  explicit ScopedRegistryUpdate(WebAppSyncBridge* sync_bridge);
  ~ScopedRegistryUpdate();

  WebAppRegistryUpdate* operator->() { return update_.get(); }

 private:
  std::unique_ptr<WebAppRegistryUpdate> update_;
  WebAppSyncBridge* const sync_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRegistryUpdate);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRY_UPDATE_H_
