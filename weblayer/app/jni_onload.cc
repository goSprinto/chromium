// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/library_loader/library_loader_hooks.h"
#include "base/logging.h"
#include "components/version_info/version_info_values.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "weblayer/app/content_main_delegate_impl.h"

namespace weblayer {

class MainDelegateImpl : public MainDelegate {
 public:
  void PreMainMessageLoopRun() override {}
  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {}
};

// This is called by the VM when the shared library is first loaded.
bool OnJNIOnLoadInit(const std::string& pak_name) {
  if (!content::android::OnJNIOnLoadInit())
    return false;
  weblayer::MainParams params;
  params.delegate = new weblayer::MainDelegateImpl;
  params.pak_name = pak_name;
  params.brand = "WebLayer";

  base::android::SetVersionNumber(PRODUCT_VERSION);
  content::SetContentMainDelegate(
      new weblayer::ContentMainDelegateImpl(params));
  return true;
}

}  // namespace weblayer
