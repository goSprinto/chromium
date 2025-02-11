// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "ios/chrome/browser/ui/passwords/password_breach_action_handler.h"

class GURL;

@protocol ApplicationCommands;
@protocol PasswordBreachConsumer;

// Object presenting the feature.
@protocol PasswordBreachPresenter <NSObject>

// Informs the presenter that the feature should dismiss.
- (void)stop;

@end

// Manages the state and interactions of the consumer.
@interface PasswordBreachMediator : NSObject <PasswordBreachActionHandler>

- (instancetype)initWithConsumer:(id<PasswordBreachConsumer>)consumer
                       presenter:(id<PasswordBreachPresenter>)presenter
                      dispatcher:(id<ApplicationCommands>)dispatcher
                             URL:(const GURL&)URL
                        leakType:(password_manager::CredentialLeakType)leakType;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_MEDIATOR_H_
