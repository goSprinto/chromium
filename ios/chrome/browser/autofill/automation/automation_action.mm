// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/automation/automation_action.h"

#import <EarlGrey/EarlGrey.h>

#include "base/guid.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutomationAction () {
  std::unique_ptr<const base::DictionaryValue> actionDictionary_;
}

@property(nonatomic, readonly)
    const std::unique_ptr<const base::DictionaryValue>& actionDictionary;

// Selects the proper subclass in the class cluster for the given type. Called
// from the class method creating the actions.
+ (Class)classForType:(NSString*)type;

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary NS_DESIGNATED_INITIALIZER;
@end

// An action that always fails.
@interface AutomationActionUnrecognized : AutomationAction
@end

// An action that simply tap on an element on the page.
// Right now this always assumes a click event of the following format:
//  {
//    "selectorType": "xpath",
//    "selector": "//*[@id=\"add-to-cart-button\"]",
//    "context": [],
//    "type": "click"
//  }
@interface AutomationActionClick : AutomationAction
@end

// An action that waits for a series of JS assertions to become true before
// continuing. We assume this action has a format resembling:
// {
//   "context": [],
//   "type": "waitFor",
//   "assertions": ["return document.querySelector().style.display ===
//   'none';"]
// }
@interface AutomationActionWaitFor : AutomationAction
@end

// An action that performs autofill on a form by selecting an element
// that is part of an autofillable form, then tapping the relevant
// autofill suggestion. We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "type": "autofill"
// }
@interface AutomationActionAutofill : AutomationAction
@end

// An action that validates a previously autofilled element.
// Confirms that the target element has is of the expected autofill field type
// and contains the specified value.
// We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@data-tl-id=\"COAC2ShpAddrFirstName\"]",
//   "context": [],
//   "expectedAutofillType": "NAME_FIRST",
//   "expectedValue": "Yuki",
//   "type": "validateField"
// }
@interface AutomationActionValidateField : AutomationAction
@end

// An action that selects a given option from a dropdown selector.
// Checks are not made to confirm that given item is a dropdown.
// We assume this action has a format resembling:
// {
//   "selectorType": "xpath",
//   "selector": "//*[@id=\"shipping-user-lookup-options\"]",
//   "context": [],
//   "index": 1,
//   "type": "select"
// }
@interface AutomationActionSelectDropdown : AutomationAction
@end

// An action that loads a web page.
// This is recorded in tandem with the actions that cause loads to
// occur (i.e. clicking on a link); therefore, this action is
// a no-op when replaying.
// We assume this action has a format resembling:
// {
//   "url": "www.google.com",
//   "type": "loadPage"
// }
@interface AutomationActionLoadPage : AutomationAction
@end

// An action that types the provided text in the specified field.
// This can be either of the "type" or "typePassword" type due to
// the two actions needing to be handled differently on desktop in order
// to ensure passwords are saved. They can be treated the same on iOS.
// We assume this action has a format resembling:
// {
//   "type": "type" OR "typePassword",
//   "selector": "//input[@autocapitalize=\"none\" and
//   @name=\"session[password]\"]", "value": "mycoolpassword",
// }
@interface AutomationActionType : AutomationAction
@end

// An action that selects the affirmative option in an open confirmation dialog.
// One main use case is selecting "Save" from the "Save password?" dialog,
// but this action cannot tell the difference between different confirmation
// dialogs, so it is multi-purpose. This action assumes that this dialog is
// already open.
// We assume this action has a format resembling:
// {
//   "type": "savePassword"
// }
@interface AutomationActionConfirmInfobar : AutomationAction
@end

@implementation AutomationAction

+ (instancetype)actionWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  const base::Value* typeValue =
      actionDictionary.FindKeyOfType("type", base::Value::Type::STRING);
  GREYAssert(typeValue, @"Type is missing in action.");

  const std::string type(typeValue->GetString());
  GREYAssert(!type.empty(), @"Type is an empty value.");

  return [[[self classForType:base::SysUTF8ToNSString(type)] alloc]
      initWithValueDictionary:actionDictionary];
}

+ (Class)classForType:(NSString*)type {
  static NSDictionary* classForType = @{
    @"click" : [AutomationActionClick class],
    @"waitFor" : [AutomationActionWaitFor class],
    @"autofill" : [AutomationActionAutofill class],
    @"validateField" : [AutomationActionValidateField class],
    @"select" : [AutomationActionSelectDropdown class],
    @"loadPage" : [AutomationActionLoadPage class],
    @"type" : [AutomationActionType class],
    @"typePassword" : [AutomationActionType class],
    @"savePassword" : [AutomationActionConfirmInfobar class],
    // More to come.
  };

  return classForType[type] ?: [AutomationActionUnrecognized class];
}

- (instancetype)initWithValueDictionary:
    (const base::DictionaryValue&)actionDictionary {
  self = [super init];
  if (self) {
    actionDictionary_ = actionDictionary.DeepCopyWithoutEmptyChildren();
  }
  return self;
}

- (void)execute {
  GREYAssert(NO, @"Should not be called!");
}

- (const std::unique_ptr<const base::DictionaryValue>&)actionDictionary {
  return actionDictionary_;
}

// A shared flow across many actions, this waits for the target element to be
// visible, scrolls it into view, then taps on it.
- (void)tapOnTarget:(ElementSelector*)selector {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  // Potentially scroll into view if below the fold.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:WebViewScrollElementToVisible(web_state, selector)];

  // Calling WebViewTapElement right after WebViewScrollElement caused flaky
  // issues with the wrong location being provided for the tap target,
  // seemingly caused by the screen not redrawing in-between these two actions.
  // We force a brief wait here to avoid this issue.
  [[GREYCondition conditionWithName:@"forced wait to allow for redraw"
                              block:^BOOL {
                                return false;
                              }] waitWithTimeout:0.1];

  // Tap on the element.
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(web_state)]
      performAction:web::WebViewTapElement(web_state, selector)];
}

// Creates a selector targeting the element specified in the action.
- (ElementSelector*)selectorForTarget {
  const std::string xpath = [self getStringFromDictionaryWithKey:"selector"];

  // Creates a selector from the action dictionary.
  ElementSelector* selector = [ElementSelector selectorWithXPathQuery:xpath];
  return selector;
}

// Returns a std::string corrensponding to the given key in the action
// dictionary. Will raise a test failure if the key is missing or the value is
// empty.
- (std::string)getStringFromDictionaryWithKey:(std::string)key {
  const base::Value* expectedTypeValue(
      self.actionDictionary->FindKeyOfType(key, base::Value::Type::STRING));
  GREYAssert(expectedTypeValue, @"%s is missing in action.", key.c_str());

  const std::string expectedType(expectedTypeValue->GetString());
  GREYAssert(!expectedType.empty(), @"%s is an empty value", key.c_str());

  return expectedType;
}

// Returns an int corrensponding to the given key in the action
// dictionary. Will raise a test failure if the key is missing or the value is
// empty.
- (int)getIntFromDictionaryWithKey:(std::string)key {
  const base::Value* expectedTypeValue(
      self.actionDictionary->FindKeyOfType(key, base::Value::Type::INTEGER));
  GREYAssert(expectedTypeValue, @"%s is missing in action.", key.c_str());

  return expectedTypeValue->GetInt();
}

// Runs the JS code passed in against the target element specified by the
// selector passed in. The target element is passed in to the JS function
// by the name "target", so example JS code is like:
// return target.value
- (id)executeJavaScript:(std::string)function
               onTarget:(ElementSelector*)selector {
  NSString* javaScript = [NSString
      stringWithFormat:@"    (function() {"
                        "      try {"
                        "        return function(target){%@}(%@);"
                        "      } catch (ex) {return 'Exception encountered "
                        "' + ex.message;}"
                        "     "
                        "    })();",
                       base::SysUTF8ToNSString(function),
                       selector.selectorScript];

  return [ChromeEarlGrey executeJavaScript:javaScript];
}

@end

@implementation AutomationActionClick

- (void)execute {
  ElementSelector* selector = [self selectorForTarget];
  [self tapOnTarget:selector];
}

@end

@implementation AutomationActionLoadPage

- (void)execute {
  // loadPage is a no-op action - perform nothing
}

@end

@implementation AutomationActionWaitFor

- (void)execute {
  const base::Value* assertionsValue(self.actionDictionary->FindKeyOfType(
      "assertions", base::Value::Type::LIST));
  GREYAssert(assertionsValue, @"Assertions key is missing in action.");

  base::span<const base::Value> assertionsValues(assertionsValue->GetList());
  GREYAssert(assertionsValues.size(), @"Assertions list is empty.");

  std::vector<std::string> state_assertions;

  for (auto const& assertionValue : assertionsValues) {
    const std::string assertionString(assertionValue.GetString());
    GREYAssert(!assertionString.empty(), @"assertionString is an empty value.");
    state_assertions.push_back(assertionString);
  }

  NSString* conditionDescription =
      @"waitFor State change hasn't completed within timeout.";
  GREYCondition* waitForElement = [GREYCondition
      conditionWithName:conditionDescription
                  block:^{
                    return
                        [self checkForJsAssertionFailures:state_assertions] ==
                        nil;
                  }];
  bool waitForCompleted =
      [waitForElement waitWithTimeout:base::test::ios::kWaitForActionTimeout];
  GREYAssertTrue(waitForCompleted, conditionDescription);
}

// Executes a vector of Javascript assertions on the webpage, returning the
// first assertion that fails to be true, or nil if all assertions are true.
- (NSString*)checkForJsAssertionFailures:
    (const std::vector<std::string>&)assertions {
  for (std::string const& assertion : assertions) {
    NSString* assertionString = base::SysUTF8ToNSString(assertion);
    NSString* javascript = [NSString stringWithFormat:@""
                                                       "    (function() {"
                                                       "      try {"
                                                       "        %@"
                                                       "      } catch (ex) {}"
                                                       "      return false;"
                                                       "    })();",
                                                      assertionString];

    NSNumber* result = base::mac::ObjCCastStrict<NSNumber>(
        [ChromeEarlGrey executeJavaScript:javascript]);

    if (![result boolValue]) {
      return assertionString;
    }
  }
  return nil;
}

@end

@implementation AutomationActionAutofill

- (void)execute {
  // The autofill profile is configured in
  // automation_egtest::prepareAutofillProfileWithValues.

  ElementSelector* selector = [self selectorForTarget];
  [self tapOnTarget:selector];

  // Tap on the autofill suggestion to perform the actual autofill.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kFormSuggestionLabelAccessibilityIdentifier)]
      performAction:grey_tap()];
}

@end

@implementation AutomationActionValidateField

- (void)execute {
  ElementSelector* selector = [self selectorForTarget];

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  NSString* expectedType = base::SysUTF8ToNSString(
      [self getStringFromDictionaryWithKey:"expectedAutofillType"]);
  NSString* expectedValue = base::SysUTF8ToNSString(
      [self getStringFromDictionaryWithKey:"expectedValue"]);

  NSString* predictionType = base::mac::ObjCCastStrict<NSString>([self
      executeJavaScript:"return target.placeholder;"
               onTarget:[self selectorForTarget]]);

  NSString* autofilledValue = base::mac::ObjCCastStrict<NSString>(
      [self executeJavaScript:"return target.value;" onTarget:selector]);

  GREYAssertEqualObjects(predictionType, expectedType,
                         @"Expected prediction type %@ but got %@",
                         expectedType, predictionType);
  GREYAssertEqualObjects(autofilledValue, expectedValue,
                         @"Expected autofilled value %@ but got %@",
                         expectedValue, autofilledValue);
}

@end

@implementation AutomationActionSelectDropdown

- (void)execute {
  ElementSelector* selector = [self selectorForTarget];

  // Wait for the element to be visible on the page.
  [ChromeEarlGrey waitForWebStateContainingElement:selector];

  int selectedIndex = [self getIntFromDictionaryWithKey:"index"];
  [self executeJavaScript:
            base::SysNSStringToUTF8([NSString
                stringWithFormat:@"target.options.selectedIndex = %d; "
                                 @"triggerOnChangeEventOnElement(target);",
                                 selectedIndex])
                 onTarget:selector];
}

@end

@implementation AutomationActionUnrecognized

- (void)execute {
  const base::Value* typeValue =
      self.actionDictionary->FindKeyOfType("type", base::Value::Type::STRING);
  const std::string type(typeValue->GetString());

  GREYAssert(NO, @"Unknown action of type %s", type.c_str());
}

@end

@implementation AutomationActionType

- (void)execute {
  ElementSelector* selector = [self selectorForTarget];
  std::string value = [self getStringFromDictionaryWithKey:"value"];
  [self executeJavaScript:
            base::SysNSStringToUTF8([NSString
                stringWithFormat:
                    @"__gCrWeb.fill.setInputElementValue(\"%s\", target);",
                    value.c_str()])
                 onTarget:selector];
}

@end

@implementation AutomationActionConfirmInfobar

- (void)execute {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kConfirmInfobarButton1AccessibilityIdentifier)]
      performAction:grey_tap()];
}

@end
