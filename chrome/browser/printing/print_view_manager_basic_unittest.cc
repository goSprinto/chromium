// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/printing_init.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/render_process_host.h"

namespace printing {

class PrintViewManagerBasicTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InitializePrinting(web_contents());
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }
};

TEST_F(PrintViewManagerBasicTest, PrintSubFrameAndDestroy) {
  auto* sub_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  ASSERT_TRUE(sub_frame);

  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());

  print_view_manager->PrintNow(sub_frame);
  EXPECT_TRUE(print_view_manager->GetPrintingRFHForTesting());

  content::RenderFrameHostTester::For(sub_frame)->Detach();
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());
}

TEST_F(PrintViewManagerBasicTest, CancelJobDuringDestruction) {
  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  ASSERT_TRUE(print_view_manager->PrintNow(main_rfh()));

  // Setup enough of a PrinterQuery to make GetPrintedPagesCount work
  auto queue = g_browser_process->print_job_manager()->queue();
  auto query = queue->CreatePrinterQuery(main_rfh()->GetProcess()->GetID(),
                                         main_rfh()->GetRoutingID());
  base::RunLoop runloop;
  query->SetSettings(GetPrintTicket(printing::kLocalPrinter),
                     runloop.QuitClosure());
  runloop.Run();
  auto cookie = query->cookie();
  queue->QueuePrinterQuery(std::move(query));

  // Fake GetPrintedPagesCount message to cause print_job to be created
  content::RenderFrameHostTester::TestOnMessageReceived(
      main_rfh(), PrintHostMsg_DidGetPrintedPagesCount(0, cookie, 1));

  DeleteContents();
}

}  // namespace printing
