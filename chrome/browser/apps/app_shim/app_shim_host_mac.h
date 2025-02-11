// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace apps {
using ShimLaunchedCallback = base::OnceCallback<void(base::Process)>;
using ShimTerminatedCallback = base::OnceClosure;
}  // namespace apps

namespace remote_cocoa {
class ApplicationHost;
}  // namespace remote_cocoa

class AppShimHostBootstrap;

// This is the counterpart to AppShimController in
// chrome/app/chrome_main_app_mode_mac.mm. The AppShimHost is owned by the
// ExtensionAppShimHandler, which implements its client interface.
class AppShimHost : public chrome::mojom::AppShimHost {
 public:
  // The interface through which the AppShimHost interacts with
  // ExtensionAppShimHandler.
  class Client {
   public:
    // Request that the handler launch the app shim process.
    virtual void OnShimLaunchRequested(
        AppShimHost* host,
        bool recreate_shims,
        apps::ShimLaunchedCallback launched_callback,
        apps::ShimTerminatedCallback terminated_callback) = 0;

    // Invoked by the shim host when the connection to the shim process is
    // closed. This is also called when we give up on trying to get a shim to
    // connect.
    virtual void OnShimProcessDisconnected(AppShimHost* host) = 0;

    // Invoked by the shim host when the shim process receives a focus event.
    // |files|, if non-empty, holds an array of files dragged onto the app
    // bundle or dock icon.
    virtual void OnShimFocus(AppShimHost* host,
                             apps::AppShimFocusType focus_type,
                             const std::vector<base::FilePath>& files) = 0;

    // Invoked when a profile is selected from the menu bar.
    virtual void OnShimSelectedProfile(AppShimHost* host,
                                       const base::FilePath& profile_path) = 0;
  };

  AppShimHost(Client* client,
              const std::string& app_id,
              const base::FilePath& profile_path,
              bool uses_remote_views);

  ~AppShimHost() override;

  bool UsesRemoteViews() const { return uses_remote_views_; }

  // Returns true if an AppShimHostBootstrap has already connected to this
  // host.
  bool HasBootstrapConnected() const;

  // Invoked to request that the shim be launched (if it has not been launched
  // already).
  void LaunchShim();

  // Invoked when the app shim has launched and connected to the browser.
  virtual void OnBootstrapConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap);

  // Functions to allow the handler to determine which app this host corresponds
  // to.
  base::FilePath GetProfilePath() const;
  std::string GetAppId() const;

  // Return the factory to use to create new widgets in the same process.
  remote_cocoa::ApplicationHost* GetRemoteCocoaApplicationHost() const;

  // Return the app shim interface.
  chrome::mojom::AppShim* GetAppShim() const;

 protected:
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // Helper function to launch the app shim process.
  void LaunchShimInternal(bool recreate_shims);

  // Called when LaunchShim has launched (or failed to launch) a process.
  void OnShimProcessLaunched(bool recreate_shims_requested,
                             base::Process shim_process);

  // Called when a shim process returned via OnShimLaunchCompleted has
  // terminated.
  void OnShimProcessTerminated(bool recreate_shims_requested);

  // chrome::mojom::AppShimHost.
  void FocusApp(apps::AppShimFocusType focus_type,
                const std::vector<base::FilePath>& files) override;
  void ProfileSelectedFromMenu(const base::FilePath& profile_path) override;

  // Weak, owns |this|.
  Client* const client_;

  mojo::Binding<chrome::mojom::AppShimHost> host_binding_;
  chrome::mojom::AppShimPtr app_shim_;
  chrome::mojom::AppShimRequest app_shim_request_;

  // Only allow LaunchShim to have any effect on the first time it is called. If
  // that launch fails, it will re-launch (requesting that the shim be
  // re-created).
  bool launch_shim_has_been_called_;

  std::unique_ptr<AppShimHostBootstrap> bootstrap_;

  std::unique_ptr<remote_cocoa::ApplicationHost> remote_cocoa_application_host_;

  std::string app_id_;
  base::FilePath profile_path_;
  const bool uses_remote_views_;

  // This class is only ever to be used on the UI thread.
  THREAD_CHECKER(thread_checker_);

  // This weak factory is used for launch callbacks only.
  base::WeakPtrFactory<AppShimHost> launch_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AppShimHost);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
