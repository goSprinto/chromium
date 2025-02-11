// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/dom_distiller_model.h"
#include "url/gurl.h"

namespace dom_distiller {

// Interface for accessing the stored DomDistiller entries.
class DomDistillerStoreInterface {
 public:
  virtual ~DomDistillerStoreInterface() {}

  // Lookup an ArticleEntry by ID or URL. Returns whether a corresponding entry
  // was found. On success, if |entry| is not null, it will contain the entry.
  virtual bool GetEntryById(const std::string& entry_id,
                            ArticleEntry* entry) = 0;
  virtual bool GetEntryByUrl(const GURL& url, ArticleEntry* entry) = 0;

  // Gets a copy of all the current entries.
  virtual std::vector<ArticleEntry> GetEntries() const = 0;
};

// Implements syncing/storing of DomDistiller entries. This keeps three
// models of the DOM distiller data in sync: the local database, sync, and the
// user (i.e. of DomDistillerStore). No changes are accepted while the local
// database is loading. Once the local database has loaded, changes from any of
// the three sources (technically just two, since changes don't come from the
// database) are handled similarly:
// 1. convert the change to a SyncChangeList.
// 2. apply that change to the in-memory model, calculating what changed
// (changes_applied) and what is missing--i.e. entries missing for a full merge,
// conflict resolution for normal changes-- (changes_missing).
// 3. send a message (possibly handled asynchronously) containing
// changes_missing to the source of the change.
// 4. send messages (possibly handled asynchronously) containing changes_applied
// to the other (i.e. non-source) two models.
// TODO(cjhopman): Support deleting entries.
class DomDistillerStore : public DomDistillerStoreInterface {
 public:
  DomDistillerStore();

  // Initializes the internal model to |initial_model|.
  DomDistillerStore(const std::vector<ArticleEntry>& initial_data);

  ~DomDistillerStore() override;

  bool GetEntryById(const std::string& entry_id, ArticleEntry* entry) override;
  bool GetEntryByUrl(const GURL& url, ArticleEntry* entry) override;
  std::vector<ArticleEntry> GetEntries() const override;

 private:
  DomDistillerModel model_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerStore);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_
