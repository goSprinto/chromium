// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_HELPER_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_HELPER_H_

#include "base/strings/string16.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace safety_tips {

// URL that the "leave site" button aborts to by default.
extern const char kSafeUrl[];

// Records a histogram for a user's interaction with a Safety Tip in the given
// |web_contents|.
void RecordSafetyTipInteractionHistogram(content::WebContents* web_contents,
                                         SafetyTipInteraction interaction);

// Invokes action when 'leave site' button is clicked, and records a histogram.
// Navigates to a safe URL, replacing the current page in the process.
void LeaveSite(content::WebContents* web_contents, const GURL& safe_url);

// Invoke action when 'Learn more' button is clicked, and records a histogram.
// Navigates to the help center URL.
void OpenHelpCenter(content::WebContents* web_contents);

// Get the titles, descriptions, and button strings or IDs needed to describe
// the applicable warning type.  Handles both Android and desktop warnings.
// |url| is the suggested URL to navigate to, and is used in formatting some
// strings.
base::string16 GetSafetyTipTitle(security_state::SafetyTipStatus warning_type,
                                 const GURL& url);
base::string16 GetSafetyTipDescription(
    security_state::SafetyTipStatus warning_type,
    const GURL& url);
int GetSafetyTipLeaveButtonId(security_state::SafetyTipStatus warning_type);

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_SAFETY_TIP_UI_HELPER_H_
