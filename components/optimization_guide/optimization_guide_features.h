// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_FEATURES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_FEATURES_H_

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/nqe/effective_connection_type.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace features {

extern const base::Feature kOptimizationHints;
extern const base::Feature kOptimizationHintsExperiments;
constexpr char kOptimizationHintsExperimentNameParam[] = "experiment_name";
extern const base::Feature kOptimizationHintsFetching;
extern const base::Feature kOptimizationGuideKeyedService;

// The maximum number of hosts that can be stored in the
// |kHintsFetcherTopHostBlacklist| dictionary pref when initialized. The top
// hosts will also be returned in order of most engaged. This prevents the most
// engaged hosts in a user's history before DataSaver being enabled from being
// requested until the user navigates to the host again.
size_t MaxHintsFetcherTopHostBlacklistSize();

// The maximum number of hosts allowed to be requested by the client to the
// remote Optimzation Guide Service.
size_t MaxHostsForOptimizationGuideServiceHintsFetch();

// The maximum number of hosts allowed to be stored as covered by the hints
// fetcher.
size_t MaxHostsForRecordingSuccessfullyCovered();

// The minimum score required to be considered a top host and be included in a
// hints fetch request.
double MinTopHostEngagementScoreThreshold();

// The amount of time a fetched hint will be considered fresh enough
// to be used and remain in the HintCacheStore.
base::TimeDelta StoredFetchedHintsFreshnessDuration();

// The duration of time after the blacklist initialization for which the low
// engagement score threshold needs to be applied. If the blacklist was
// initialized more than DurationApplyLowEngagementScoreThreshold() ago, then
// the low engagement score threshold need not be applied.
base::TimeDelta DurationApplyLowEngagementScoreThreshold();

// The API key for the One Platform Optimization Guide Service.
std::string GetOptimizationGuideServiceAPIKey();

// The host for the One Platform Optimization Guide Service.
GURL GetOptimizationGuideServiceURL();

// Whether server optimization hints are enabled.
bool IsOptimizationHintsEnabled();

// Returns true if the feature to fetch hints from the remote Optimization Guide
// Service is enabled.
bool IsHintsFetchingEnabled();

// Returns true if the initialization of the Optimization Guide Keyed Service is
// enabled.
bool IsOptimizationGuideKeyedServiceEnabled();

// The maximum data byte size for a server-provided bloom filter. This is
// a client-side safety limit for RAM use in case server sends too large of
// a bloom filter.
int MaxServerBloomFilterByteSize();

// Maximum effective connection type at which hints can be fetched for
// navigations in real-time. Returns null if the hints fetching for navigations
// is disabled.
base::Optional<net::EffectiveConnectionType>
GetMaxEffectiveConnectionTypeForNavigationHintsFetch();

}  // namespace features
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_FEATURES_H_
