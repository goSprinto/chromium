// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

using content::NavigationEntry;

SupervisedUserNavigationObserver::~SupervisedUserNavigationObserver() {
  supervised_user_service_->RemoveObserver(this);
}

SupervisedUserNavigationObserver::SupervisedUserNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), binding_(web_contents, this) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  supervised_user_service_ =
      SupervisedUserServiceFactory::GetForProfile(profile);
  url_filter_ = supervised_user_service_->GetURLFilter();
  supervised_user_service_->AddObserver(this);
}

// static
void SupervisedUserNavigationObserver::OnRequestBlocked(
    content::WebContents* web_contents,
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    int64_t navigation_id,
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback) {
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents);

  // Cancel the navigation if there is no navigation observer.
  if (!navigation_observer) {
    callback.Run(
        SupervisedUserNavigationThrottle::CallbackActions::kCancelNavigation);
    return;
  }

  navigation_observer->OnRequestBlockedInternal(url, reason, navigation_id,
                                                callback);
}

void SupervisedUserNavigationObserver::UpdateMainFrameFilteringStatus(
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason) {
  main_frame_filtering_behavior_ = behavior;
  main_frame_filtering_behavior_reason_ = reason;
}

void SupervisedUserNavigationObserver::DidFinishNavigation(
      content::NavigationHandle* navigation_handle) {
  // If this is a different navigation than the one that triggered the
  // interstitial, remove the interstitial.
  if (interstitial_ &&
      navigation_handle->GetNavigationId() != interstitial_navigation_id_) {
    OnInterstitialDone();
  }

  // Only filter same page navigations (eg. pushState/popState); others will
  // have been filtered by the NavigationThrottle.
  if (navigation_handle->IsSameDocument() &&
      navigation_handle->IsInMainFrame()) {
    url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
        web_contents()->GetLastCommittedURL(),
        base::BindOnce(
            &SupervisedUserNavigationObserver::URLFilterCheckCallback,
            weak_ptr_factory_.GetWeakPtr(), navigation_handle->GetURL()));
  }
}

void SupervisedUserNavigationObserver::OnURLFilterChanged() {
  url_filter_->GetFilteringBehaviorForURLWithAsyncChecks(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&SupervisedUserNavigationObserver::URLFilterCheckCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents()->GetLastCommittedURL()));
}

void SupervisedUserNavigationObserver::OnRequestBlockedInternal(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    int64_t navigation_id,
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback) {
  // TODO(bauerb): Use SaneTime when available.
  base::Time timestamp = base::Time::Now();
  // Create a history entry for the attempt and mark it as such.  This history
  // entry should be marked as "not hidden" so the user can see attempted but
  // blocked navigations.  (This is in contrast to the normal behavior, wherein
  // Chrome marks navigations that result in an error as hidden.)  This is to
  // show the user the same thing that the custodian will see on the dashboard
  // (where it gets via a different mechanism unrelated to history).
  history::HistoryAddPageArgs add_page_args(
      url, timestamp, history::ContextIDForWebContents(web_contents()), 0, url,
      history::RedirectList(), ui::PAGE_TRANSITION_BLOCKED, false,
      history::SOURCE_BROWSED, false, true);

  // Add the entry to the history database.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);

  // |history_service| is null if saving history is disabled.
  if (history_service)
    history_service->AddPage(add_page_args);

  std::unique_ptr<NavigationEntry> entry = NavigationEntry::Create();
  entry->SetVirtualURL(url);
  entry->SetTimestamp(timestamp);
  auto serialized_entry = std::make_unique<sessions::SerializedNavigationEntry>(
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          blocked_navigations_.size(), entry.get()));
  blocked_navigations_.push_back(std::move(serialized_entry));

  // Show the interstitial.
  const bool initial_page_load = true;
  MaybeShowInterstitial(url, reason, initial_page_load, navigation_id,
                        callback);
}

void SupervisedUserNavigationObserver::URLFilterCheckCallback(
    const GURL& url,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool uncertain) {
  // If the page has been changed in the meantime, we can exit.
  if (url != web_contents()->GetLastCommittedURL())
    return;

  bool is_showing_interstitial = interstitial_.get() != nullptr;
  bool should_show_interstitial =
      behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK;

  if (is_showing_interstitial != should_show_interstitial)
    web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
}

void SupervisedUserNavigationObserver::MaybeShowInterstitial(
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool initial_page_load,
    int64_t navigation_id,
    const base::Callback<
        void(SupervisedUserNavigationThrottle::CallbackActions)>& callback) {
  interstitial_navigation_id_ = navigation_id;
  interstitial_ =
      SupervisedUserInterstitial::Create(web_contents(), url, reason);
  callback.Run(SupervisedUserNavigationThrottle::CallbackActions::
                   kCancelWithInterstitial);
}

void SupervisedUserNavigationObserver::OnInterstitialDone() {
  interstitial_.reset();
}

void SupervisedUserNavigationObserver::GoBack() {
  if (interstitial_)
    interstitial_->GoBack();
}

void SupervisedUserNavigationObserver::RequestPermission(
    RequestPermissionCallback callback) {
  if (interstitial_)
    interstitial_->RequestPermission(std::move(callback));
}

void SupervisedUserNavigationObserver::Feedback() {
  if (interstitial_)
    interstitial_->ShowFeedback();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SupervisedUserNavigationObserver)
