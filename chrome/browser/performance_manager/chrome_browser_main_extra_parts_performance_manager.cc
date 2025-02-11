// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/chrome_browser_main_extra_parts_performance_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/performance_manager/browser_child_process_watcher.h"
#include "chrome/browser/performance_manager/decorators/frozen_frame_aggregator.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"
#include "chrome/browser/performance_manager/decorators/process_metrics_decorator.h"
#include "chrome/browser/performance_manager/graph/policies/policy_features.h"
#include "chrome/browser/performance_manager/graph/policies/working_set_trimmer_policy.h"
#include "chrome/browser/performance_manager/observers/isolation_context_metrics.h"
#include "chrome/browser/performance_manager/observers/metrics_collector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/render_process_user_data.h"
#include "components/performance_manager/shared_worker_watcher.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"

#if defined(OS_LINUX)
#include "base/allocator/buildflags.h"
#if BUILDFLAG(USE_TCMALLOC)
#include "chrome/browser/performance_manager/graph/policies/dynamic_tcmalloc_policy_linux.h"
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // defined(OS_LINUX)

ChromeBrowserMainExtraPartsPerformanceManager::
    ChromeBrowserMainExtraPartsPerformanceManager() = default;
ChromeBrowserMainExtraPartsPerformanceManager::
    ~ChromeBrowserMainExtraPartsPerformanceManager() = default;

// static
void ChromeBrowserMainExtraPartsPerformanceManager::
    CreateDefaultPoliciesAndDecorators(performance_manager::GraphImpl* graph) {
  graph->PassToGraph(std::make_unique<performance_manager::PageAggregator>());
  graph->PassToGraph(
      std::make_unique<performance_manager::FrozenFrameAggregator>());
  graph->PassToGraph(
      std::make_unique<performance_manager::PageAlmostIdleDecorator>());
  graph->PassToGraph(
      std::make_unique<performance_manager::IsolationContextMetrics>());
  graph->PassToGraph(std::make_unique<performance_manager::MetricsCollector>());
  graph->PassToGraph(
      std::make_unique<performance_manager::ProcessMetricsDecorator>());

  if (performance_manager::policies::WorkingSetTrimmerPolicy::
          PlatformSupportsWorkingSetTrim()) {
    graph->PassToGraph(performance_manager::policies::WorkingSetTrimmerPolicy::
                           CreatePolicyForPlatform());
  }

#if defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kDynamicTcmallocTuning)) {
    graph->PassToGraph(std::make_unique<
                       performance_manager::policies::DynamicTcmallocPolicy>());
  }
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // defined(OS_LINUX)
}

void ChromeBrowserMainExtraPartsPerformanceManager::PostCreateThreads() {
  performance_manager_ = performance_manager::PerformanceManagerImpl::Create(
      base::BindOnce(&ChromeBrowserMainExtraPartsPerformanceManager::
                         CreateDefaultPoliciesAndDecorators));
  browser_child_process_watcher_ =
      std::make_unique<performance_manager::BrowserChildProcessWatcher>();
  browser_child_process_watcher_->Initialize();

  // There are no existing loaded profiles.
  DCHECK(g_browser_process->profile_manager()->GetLoadedProfiles().empty());

  // Register for profile events to observe them as they come and go.
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());
}

void ChromeBrowserMainExtraPartsPerformanceManager::PostMainMessageLoopRun() {
  // Release all graph nodes before destroying the performance manager.
  // First release the browser and GPU process nodes.
  browser_child_process_watcher_->TearDown();
  browser_child_process_watcher_.reset();

  // Clear up the worker nodes.
  for (auto& shared_worker_watcher : shared_worker_watchers_)
    shared_worker_watcher.second->TearDown();
  shared_worker_watchers_.clear();

  // There may still be WebContents with attached tab helpers at this point in
  // time, and there's no convenient later call-out to destroy the performance
  // manager. To release the page and frame nodes, detach the tab helpers
  // from any existing WebContents.
  performance_manager::PerformanceManagerTabHelper::DetachAndDestroyAll();

  // Then the render process nodes. These have to be destroyed after the
  // frame nodes.
  performance_manager::RenderProcessUserData::DetachAndDestroyAll();

  performance_manager::PerformanceManagerImpl::Destroy(
      std::move(performance_manager_));

  // Unregister to profile notifications so as to not create more worker
  // watchers after this point.
  notification_registrar_.RemoveAll();
}

void ChromeBrowserMainExtraPartsPerformanceManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      CreateSharedWorkerWatcher(profile);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      DeleteSharedWorkerWatcher(profile);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ChromeBrowserMainExtraPartsPerformanceManager::CreateSharedWorkerWatcher(
    Profile* profile) {
  auto shared_worker_watcher =
      std::make_unique<performance_manager::SharedWorkerWatcher>(
          profile->UniqueId(),
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetSharedWorkerService(),
          &process_node_source_, &frame_node_source_);

  bool inserted = shared_worker_watchers_
                      .insert({profile, std::move(shared_worker_watcher)})
                      .second;
  DCHECK(inserted);
}

void ChromeBrowserMainExtraPartsPerformanceManager::DeleteSharedWorkerWatcher(
    Profile* profile) {
  auto it = shared_worker_watchers_.find(profile);
  DCHECK(it != shared_worker_watchers_.end());
  it->second->TearDown();
  shared_worker_watchers_.erase(it);
}
