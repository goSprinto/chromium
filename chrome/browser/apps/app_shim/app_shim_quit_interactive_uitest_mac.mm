// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests behavior when quitting apps with app shims.

#import <Cocoa/Cocoa.h>
#include <unistd.h>

#include <vector>

#include "apps/switches.h"
#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/events/test/cocoa_test_event_utils.h"

using extensions::PlatformAppBrowserTest;

namespace apps {

namespace {

// Test class used to expose protected methods of AppShimHostBootstrap.
class TestAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  TestAppShimHostBootstrap() : AppShimHostBootstrap(getpid()) {}
  using AppShimHostBootstrap::OnShimConnected;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppShimHostBootstrap);
};

// Starts an app without a browser window using --load_and_launch_app and
// --silent_launch.
class AppShimQuitTest : public PlatformAppBrowserTest {
 protected:
  AppShimQuitTest() {}

  void SetUpAppShim() {
    ASSERT_EQ(0u, [[NSApp windows] count]);
    ExtensionTestMessageListener launched_listener("Launched", false);
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
    ASSERT_EQ(1u, [[NSApp windows] count]);

    handler_ = g_browser_process->platform_part()
                   ->app_shim_listener()
                   ->extension_app_shim_handler();

    // Attach a host for the app.
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());
    extension_id_ =
        GetExtensionByPath(registry->enabled_extensions(), app_path_)->id();
    chrome::mojom::AppShimHostPtr host_ptr;
    auto app_shim_info = chrome::mojom::AppShimInfo::New();
    app_shim_info->profile_path = profile()->GetPath().BaseName();
    app_shim_info->app_id = extension_id_;
    app_shim_info->app_url = GURL("https://example.com");
    app_shim_info->launch_type = apps::APP_SHIM_LAUNCH_REGISTER_ONLY;
    (new TestAppShimHostBootstrap)
        ->OnShimConnected(mojo::MakeRequest(&host_ptr),
                          std::move(app_shim_info),
                          base::BindOnce(&AppShimQuitTest::DoShimLaunchDone,
                                         base::Unretained(this)));

    // Focus the app window.
    NSWindow* window = [[NSApp windows] objectAtIndex:0];
    EXPECT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
    content::RunAllPendingInMessageLoop();
  }

  void DoShimLaunchDone(apps::AppShimLaunchResult result,
                        chrome::mojom::AppShimRequest app_shim_request) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    // Simulate an app shim initiated launch, i.e. launch app but not browser.
    app_path_ = test_data_dir_
        .AppendASCII("platform_apps")
        .AppendASCII("minimal");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp,
                                     app_path_.value());
    command_line->AppendSwitch(switches::kSilentLaunch);
  }

  base::FilePath app_path_;
  ExtensionAppShimHandler* handler_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(AppShimQuitTest);
};

}  // namespace

// Test that closing an app with Cmd+Q when no browsers have ever been open
// terminates Chrome.
IN_PROC_BROWSER_TEST_F(AppShimQuitTest, QuitWithKeyEvent) {
  SetUpAppShim();

  // Simulate a Cmd+Q event.
  NSWindow* window = [[NSApp windows] objectAtIndex:0];
  NSEvent* event = cocoa_test_event_utils::KeyEventWithKeyCode(
      0, 'q', NSKeyDown, NSCommandKeyMask);
  [window postEvent:event
            atStart:NO];

  // This will time out if the event above does not terminate Chrome.
  RunUntilBrowserProcessQuits();

  EXPECT_FALSE(handler_->FindHost(profile(), extension_id_));
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}

}  // namespace apps
