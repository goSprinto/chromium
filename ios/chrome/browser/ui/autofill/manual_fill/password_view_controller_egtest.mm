// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYKeyboard.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/all_password_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/web_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::CancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;
using chrome_test_util::TapWebElementWithIdInFrame;

namespace {

const char kFormElementUsername[] = "username";
const char kFormElementPassword[] = "password";

const char kExampleUsername[] = "concrete username";
const char kExamplePassword[] = "concrete password";

const char kFormHTMLFile[] = "/username_password_field_form.html";
const char kIFrameHTMLFile[] = "/iframe_form.html";

// Returns a matcher for the password icon in the keyboard accessory bar.
id<GREYMatcher> PasswordIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryPasswordAccessibilityIdentifier);
}

id<GREYMatcher> KeyboardIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryKeyboardAccessibilityIdentifier);
}

// Returns a matcher for the password table view in manual fallback.
id<GREYMatcher> PasswordTableViewMatcher() {
  return grey_accessibilityID(
      manual_fill::kPasswordTableViewAccessibilityIdentifier);
}

// Returns a matcher for the password search bar in manual fallback.
id<GREYMatcher> PasswordSearchBarMatcher() {
  return grey_accessibilityID(
      manual_fill::kPasswordSearchBarAccessibilityIdentifier);
}

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManagePasswordsMatcher() {
  return grey_accessibilityID(
      manual_fill::ManagePasswordsAccessibilityIdentifier);
}

// Returns a matcher for the button to open all passwords in manual fallback.
id<GREYMatcher> OtherPasswordsMatcher() {
  return grey_accessibilityID(
      manual_fill::OtherPasswordsAccessibilityIdentifier);
}

id<GREYMatcher> OtherPasswordsDismissMatcher() {
  return grey_accessibilityID(
      manual_fill::kPasswordDoneButtonAccessibilityIdentifier);
}

// Returns a matcher for the example username in the list.
id<GREYMatcher> UsernameButtonMatcher() {
  return grey_buttonTitle(base::SysUTF8ToNSString(kExampleUsername));
}

// Returns a matcher for the example password in the list.
id<GREYMatcher> PasswordButtonMatcher() {
  return grey_buttonTitle(kMaskedPasswordTitle);
}

// Returns a matcher for the password settings collection view.
id<GREYMatcher> PasswordSettingsMatcher() {
  return grey_accessibilityID(kPasswordsTableViewId);
}

// Returns a matcher for the search bar in password settings.
id<GREYMatcher> PasswordSettingsSearchMatcher() {
  return grey_accessibilityID(kPasswordsSearchBarId);
}

// Matcher for the not secure website alert.
id<GREYMatcher> NotSecureWebsiteAlert() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
}

// Returns a matcher for the PasswordTableView window.
id<GREYMatcher> PasswordTableViewWindowMatcher() {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(PasswordTableViewMatcher());
  return grey_allOf(classMatcher, parentMatcher, nil);
}

// Returns the matcher for an enabled cancel button in a navigation bar.
id<GREYMatcher> NavigationBarCancelMatcher() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), CancelButton(),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Gets the current password store.
scoped_refptr<password_manager::PasswordStore> GetPasswordStore() {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, and should not run in Incognito
  // context. Therefore IMPLICIT_ACCESS is used to let the test fail if in
  // Incognito context.
  return IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// This class is used to obtain results from the PasswordStore and hence both
// check the success of store updates and ensure that store has finished
// processing.
class TestStoreConsumer : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> obtained) override {
    obtained_ = std::move(obtained);
  }

  const std::vector<autofill::PasswordForm>& GetStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordStore()->GetAllLogins(this);
    bool responded = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^bool {
      return !AreObtainedReset();
    });
    GREYAssert(responded, @"Obtaining fillable items took too long.");
    AppendObtainedToResults();
    return results_;
  }

 private:
  // Puts |obtained_| in a known state not corresponding to any PasswordStore
  // state.
  void ResetObtained() {
    obtained_.clear();
    obtained_.emplace_back(nullptr);
  }

  // Returns true if |obtained_| are in the reset state.
  bool AreObtainedReset() { return obtained_.size() == 1 && !obtained_[0]; }

  void AppendObtainedToResults() {
    for (const auto& source : obtained_) {
      results_.emplace_back(*source);
    }
    ResetObtained();
  }

  // Temporary cache of obtained store results.
  std::vector<std::unique_ptr<autofill::PasswordForm>> obtained_;

  // Combination of fillable and blacklisted credentials from the store.
  std::vector<autofill::PasswordForm> results_;
};

// Saves |form| to the password store and waits until the async processing is
// done.
void SaveToPasswordStore(const autofill::PasswordForm& form) {
  GetPasswordStore()->AddLogin(form);
  // When we retrieve the form from the store, |from_store| should be set.
  autofill::PasswordForm expected_form = form;
  expected_form.from_store = autofill::PasswordForm::Store::kProfileStore;
  // Check the result and ensure PasswordStore processed this.
  TestStoreConsumer consumer;
  for (const auto& result : consumer.GetStoreResults()) {
    if (result == expected_form)
      return;
  }
  GREYFail(@"Stored form was not found in the PasswordStore results.");
}

// Saves an example form in the store.
void SaveExamplePasswordForm() {
  autofill::PasswordForm example;
  example.username_value = base::ASCIIToUTF16(kExampleUsername);
  example.password_value = base::ASCIIToUTF16(kExamplePassword);
  example.origin = GURL("https://example.com/");
  example.signon_realm = example.origin.spec();
  SaveToPasswordStore(example);
}

// Saves an example form in the storefor the passed URL.
void SaveLocalPasswordForm(const GURL& url) {
  autofill::PasswordForm localForm;
  localForm.username_value = base::ASCIIToUTF16(kExampleUsername);
  localForm.password_value = base::ASCIIToUTF16(kExamplePassword);
  localForm.origin = url;
  localForm.signon_realm = localForm.origin.spec();
  SaveToPasswordStore(localForm);
}

// Removes all credentials stored.
void ClearPasswordStore() {
  GetPasswordStore()->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                                 base::Closure());
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"PasswordStore was not cleared.");
}

// Polls the JavaScript query |java_script_condition| until the returned
// |boolValue| is YES with a kWaitForActionTimeout timeout.
BOOL WaitForJavaScriptCondition(NSString* java_script_condition) {
  auto verify_block = ^BOOL {
    id value = chrome_test_util::ExecuteJavaScript(java_script_condition, nil);
    return [value isEqual:@YES];
  };
  NSTimeInterval timeout = base::test::ios::kWaitForActionTimeout;
  NSString* condition_name = [NSString
      stringWithFormat:@"Wait for JS condition: %@", java_script_condition];
  GREYCondition* condition =
      [GREYCondition conditionWithName:condition_name block:verify_block];
  return [condition waitWithTimeout:timeout];
}

}  // namespace

// Integration Tests for Mannual Fallback Passwords View Controller.
@interface PasswordViewControllerTestCase : ChromeTestCase
@end

@implementation PasswordViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
  SaveExamplePasswordForm();
}

- (void)tearDown {
  ClearPasswordStore();
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  [super tearDown];
}

// Tests that the passwords view controller appears on screen.
- (void)testPasswordsViewControllerIsPresented {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the passwords view controller contains the "Manage Passwords..."
// action.
- (void)testPasswordsViewControllerContainsManagePasswordsAction {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller contains the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the "Manage Passwords..." action works.
- (void)testManagePasswordsActionOpensPasswordSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:PasswordSettingsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Manage Passwords..." leaves the keyboard and the
// icons in the right state.
- (void)testPasswordsStateAfterPresentingManagePasswords {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:PasswordSettingsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the password view.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Use Other Password..." action works.
- (void)testUseOtherPasswordActionOpens {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the use other passwords opened.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Use Other Password..." leaves the view and icons
// in the right state.
- (void)testPasswordsStateAfterPresentingUseOtherPassword {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the use other passwords opened.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the password view.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is not present when presenting UI.
- (void)testPasswordControllerPauses {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:PasswordSettingsSearchMatcher()]
      performAction:grey_tap()];

  // Verify keyboard is shown without the password controller.
  GREYAssertTrue([GREYKeyboard isKeyboardShown], @"Keyboard Should be Shown");
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is resumed after selecting other
// password.
// TODO(crbug.com/981922): Re-enable this test due to failing DB call.
- (void)DISABLED_testPasswordControllerResumes {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Other Passwords..." action.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:PasswordSearchBarMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([GREYKeyboard isKeyboardShown], @"Keyboard Should be Shown");

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the password list to disappear. Using the search bar, since the
  // popover doesn't have it.
  [[EarlGrey selectElementWithMatcher:PasswordSearchBarMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Only on iOS 11.3 it is certain that on iPhones the keyboard is back. On iOS
  // 11.0-11.2, it varies by device and version.
  if ([GREYKeyboard isKeyboardShown]) {
    [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Tests that the Password View Controller is resumed after dismissing "Other
// Passwords".
// TODO(crbug.com/984977): Support this behavior again.
- (void)DISABLED_testPasswordControllerResumesWhenOtherPasswordsDismiss {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Other Passwords..." action.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Dismiss the Other Passwords view.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsDismissMatcher()]
      performAction:grey_tap()];

  // Wait for the password list to disappear. Using the search bar, since the
  // popover doesn't have it.
  [[EarlGrey selectElementWithMatcher:PasswordSearchBarMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Only on iOS 11.3 it is certain that on iPhones the keyboard is back. On iOS
  // 11.0-11.2, it varies by device and version.
  if ([GREYKeyboard isKeyboardShown]) {
    [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Test that after searching in "Use Other Password" and selecting an option,
// the screen is dismissed.
- (void)testOtherPasswordsDismissAfterSearch {
  // This test is failing on a legacy bot. Return early if running there.
  if (![ChromeEarlGrey isSlimNavigationManagerEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for Slim Navigation off.");
  }

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap "Use Other Password...".
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Verify "Use Other Passwords" opened.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the password search bar.
  [[EarlGrey selectElementWithMatcher:PasswordSearchBarMatcher()]
      performAction:grey_typeText(@"user")];

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Verify the password list disappears.
  [[EarlGrey selectElementWithMatcher:PasswordSearchBarMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that the Password View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissPasswordController {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The keyboard icon is never present in iPads.
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password View Controller is dismissed when tapping the outside
// the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissPasswordController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the password controller table view is not visible and the password
  // icon is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_interactable()];
  // Verify the interaction status of the password icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
}

// Tests that the Password View Controller is dismissed when tapping the
// keyboard.
// TODO(crbug.com/909629): started to be flaky and sometimes opens full list
// when typing text.
- (void)DISABLED_testTappingKeyboardDismissPasswordControllerPopOver {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      performAction:grey_typeText(@"text")];

  // Verify the password controller table view and the password icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that after switching fields the content size of the table view didn't
// grow.
- (void)testPasswordControllerKeepsRightSize {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the "Manage Passwords..." is on screen.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the second element.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify the "Manage Passwords..." is on screen.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Password View Controller stays on rotation.
- (void)testPasswordControllerSupportsRotation {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];

  // Verify the password controller table view is still visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that content is injected in iframe messaging.
- (void)testPasswordControllerSupportsIFrameMessaging {
  // Iframe messaging is not supported on iOS < 11.3.
  if (!base::ios::IsRunningOnOrLater(11, 3, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iOS < 11.3");
  }

  const GURL URL = self.testServer->GetURL(kIFrameHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"iFrame"];
  SaveLocalPasswordForm(URL);

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithIdInFrame(kFormElementUsername, 0)];

  // Wait for the accessory icon to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:
          @"window.frames[0].document.getElementById('%s').value === '%s'",
          kFormElementUsername, kExampleUsername];
  XCTAssertTrue(WaitForJavaScriptCondition(javaScriptCondition));
}

// Tests that an alert is shown when trying to fill a password in an unsecure
// field.
- (void)testPasswordControllerPresentsUnsecureAlert {
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  SaveLocalPasswordForm(URL);

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a password.
  [[EarlGrey selectElementWithMatcher:PasswordButtonMatcher()]
      performAction:grey_tap()];

  // Look for the alert.
  [[EarlGrey selectElementWithMatcher:NotSecureWebsiteAlert()]
      assertWithMatcher:grey_not(grey_nil())];
}

// Tests that the password icon is hidden when no passwords are available.
- (void)testPasswordIconIsNotVisibleWhenPasswordStoreEmpty {
  ClearPasswordStore();

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the keyboard to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Assert the password icon is not visible.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Store one password.
  SaveExamplePasswordForm();

  // Tap another field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementPassword)];

  // Assert the password icon is visible now.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
