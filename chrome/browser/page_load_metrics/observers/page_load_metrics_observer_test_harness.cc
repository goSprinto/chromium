// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace page_load_metrics {

PageLoadMetricsObserverTestHarness::PageLoadMetricsObserverTestHarness()
    : ChromeRenderViewHostTestHarness() {}

PageLoadMetricsObserverTestHarness::~PageLoadMetricsObserverTestHarness() {}

void PageLoadMetricsObserverTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("http://www.google.com"));
  // Page load metrics depends on UKM source URLs being recorded, so make sure
  // the SourceUrlRecorderWebContentsObserver is instantiated.
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  tester_ = std::make_unique<PageLoadMetricsObserverTester>(
      web_contents(), this,
      base::BindRepeating(
          &PageLoadMetricsObserverTestHarness::RegisterObservers,
          base::Unretained(this)));
  web_contents()->WasShown();
}

void PageLoadMetricsObserverTestHarness::StartNavigation(const GURL& gurl) {
  tester_->StartNavigation(gurl);
}

void PageLoadMetricsObserverTestHarness::SimulateTimingUpdate(
    const mojom::PageLoadTiming& timing) {
  tester_->SimulateTimingUpdate(timing);
}

void PageLoadMetricsObserverTestHarness::SimulateTimingUpdate(
    const mojom::PageLoadTiming& timing,
    content::RenderFrameHost* rfh) {
  tester_->SimulateTimingUpdate(timing, rfh);
}

void PageLoadMetricsObserverTestHarness::SimulateTimingAndMetadataUpdate(
    const mojom::PageLoadTiming& timing,
    const mojom::PageLoadMetadata& metadata) {
  tester_->SimulateTimingAndMetadataUpdate(timing, metadata);
}

void PageLoadMetricsObserverTestHarness::SimulateCpuTimingUpdate(
    const mojom::CpuTiming& cpu_timing) {
  tester_->SimulateCpuTimingUpdate(cpu_timing);
}

void PageLoadMetricsObserverTestHarness::SimulateMetadataUpdate(
    const mojom::PageLoadMetadata& metadata,
    content::RenderFrameHost* rfh) {
  tester_->SimulateMetadataUpdate(metadata, rfh);
}

void PageLoadMetricsObserverTestHarness::SimulateResourceDataUseUpdate(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  tester_->SimulateResourceDataUseUpdate(resources);
}

void PageLoadMetricsObserverTestHarness::SimulateResourceDataUseUpdate(
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    content::RenderFrameHost* render_frame_host) {
  tester_->SimulateResourceDataUseUpdate(resources, render_frame_host);
}

void PageLoadMetricsObserverTestHarness::SimulateFeaturesUpdate(
    const mojom::PageLoadFeatures& new_features) {
  tester_->SimulateFeaturesUpdate(new_features);
}

void PageLoadMetricsObserverTestHarness::SimulateRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data) {
  tester_->SimulateRenderDataUpdate(render_data);
}

void PageLoadMetricsObserverTestHarness::SimulateRenderDataUpdate(
    const mojom::FrameRenderDataUpdate& render_data,
    content::RenderFrameHost* render_frame_host) {
  tester_->SimulateRenderDataUpdate(render_data, render_frame_host);
}

void PageLoadMetricsObserverTestHarness::SimulateLoadedResource(
    const ExtraRequestCompleteInfo& info) {
  tester_->SimulateLoadedResource(info, content::GlobalRequestID());
}

void PageLoadMetricsObserverTestHarness::SimulateLoadedResource(
    const ExtraRequestCompleteInfo& info,
    const content::GlobalRequestID& request_id) {
  tester_->SimulateLoadedResource(info, request_id);
}

void PageLoadMetricsObserverTestHarness::SimulateInputEvent(
    const blink::WebInputEvent& event) {
  tester_->SimulateInputEvent(event);
}

void PageLoadMetricsObserverTestHarness::SimulateAppEnterBackground() {
  tester_->SimulateAppEnterBackground();
}

void PageLoadMetricsObserverTestHarness::SimulateMediaPlayed() {
  tester_->SimulateMediaPlayed();
}

void PageLoadMetricsObserverTestHarness::SimulateCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  tester_->SimulateCookiesRead(url, first_party_url, cookie_list,
                               blocked_by_policy);
}

void PageLoadMetricsObserverTestHarness::SimulateCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  tester_->SimulateCookieChange(url, first_party_url, cookie,
                                blocked_by_policy);
}

void PageLoadMetricsObserverTestHarness::SimulateDomStorageAccess(
    const GURL& url,
    const GURL& first_party_url,
    bool local,
    bool blocked_by_policy) {
  tester_->SimulateDomStorageAccess(url, first_party_url, local,
                                    blocked_by_policy);
}

const base::HistogramTester&
PageLoadMetricsObserverTestHarness::histogram_tester() const {
  return tester_->histogram_tester();
}

MetricsWebContentsObserver* PageLoadMetricsObserverTestHarness::observer()
    const {
  return tester_->observer();
}

const ukm::TestAutoSetUkmRecorder&
PageLoadMetricsObserverTestHarness::test_ukm_recorder() const {
  return tester_->test_ukm_recorder();
}

const PageLoadMetricsObserverDelegate&
PageLoadMetricsObserverTestHarness::GetDelegateForCommittedLoad() const {
  return tester_->GetDelegateForCommittedLoad();
}

void PageLoadMetricsObserverTestHarness::NavigateWithPageTransitionAndCommit(
    const GURL& url,
    ui::PageTransition transition) {
  tester_->NavigateWithPageTransitionAndCommit(url, transition);
}

void PageLoadMetricsObserverTestHarness::NavigateToUntrackedUrl() {
  tester_->NavigateToUntrackedUrl();
}

const char PageLoadMetricsObserverTestHarness::kResourceUrl[] =
    "https://www.example.com/resource";

}  // namespace page_load_metrics
