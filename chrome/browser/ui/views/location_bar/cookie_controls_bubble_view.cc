// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"

#include <memory>
#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
// Singleton instance of the cookie bubble. The cookie bubble can only be
// shown on the active browser window, so there is no case in which it will be
// shown twice at the same time.
static CookieControlsBubbleView* g_instance;
}  // namespace

// static
void CookieControlsBubbleView::ShowBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    content::WebContents* web_contents,
    CookieControlsController* controller,
    CookieControlsController::Status status) {
  DCHECK(web_contents);
  if (g_instance)
    return;

  g_instance =
      new CookieControlsBubbleView(anchor_view, web_contents, controller);
  g_instance->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(g_instance);
  controller->Update(web_contents);
  bubble_widget->Show();
}

// static
CookieControlsBubbleView* CookieControlsBubbleView::GetCookieBubble() {
  return g_instance;
}

void CookieControlsBubbleView::OnStatusChanged(
    CookieControlsController::Status new_status) {
  if (status_ == new_status)
    return;

  show_disable_cookie_blocking_ui_ = false;
  status_ = new_status;
  UpdateUi();
}

void CookieControlsBubbleView::OnBlockedCookiesCountChanged(
    int blocked_cookies) {
  // The blocked cookie count changes quite frequently, so avoid unnecessary
  // UI updates and unnecessarily calling GetBlockedDomainCount() if possible.
  if (blocked_cookies_ == blocked_cookies)
    return;

  bool has_blocked_changed =
      !blocked_cookies_ || (*blocked_cookies_ > 0) != (blocked_cookies > 0);
  blocked_cookies_ = blocked_cookies;

  text_->SetText(
      l10n_util::GetPluralStringFUTF16(IDS_COOKIE_CONTROLS_BLOCKED_MESSAGE,
                                       controller_->GetBlockedDomainCount()));

  // If this only incremented the number of blocked sites, no UI update is
  // necessary besides the |text_| text.
  if (has_blocked_changed)
    UpdateUi();
}

CookieControlsBubbleView::CookieControlsBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    CookieControlsController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  observer_.Add(controller);
}

CookieControlsBubbleView::~CookieControlsBubbleView() = default;

void CookieControlsBubbleView::UpdateUi() {
  if (status_ == CookieControlsController::Status::kDisabled) {
    CloseBubble();
    return;
  }

  DialogModelChanged();
  GetBubbleFrameView()->UpdateWindowTitle();

  bool has_blocked_cookies = blocked_cookies_ > 0;

  not_working_link_->SetVisible(false);
  text_->SetVisible(false);
  header_view_->SetVisible(false);

  if (show_disable_cookie_blocking_ui_) {
    text_->SetVisible(true);
    text_->SetText(
        l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_NOT_WORKING_DESCRIPTION));
  } else if (status_ == CookieControlsController::Status::kEnabled) {
    header_view_->SetVisible(true);
    header_view_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            has_blocked_cookies ? IDR_COOKIE_BLOCKING_ON_HEADER
                                : IDR_COOKIE_BLOCKING_INACTIVE_HEADER));
    text_->SetVisible(true);
    not_working_link_->SetVisible(has_blocked_cookies);
    blocked_cookies_.reset();
  } else {
    DCHECK_EQ(status_, CookieControlsController::Status::kDisabledForSite);
    header_view_->SetVisible(true);
    header_view_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_COOKIE_BLOCKING_OFF_HEADER));
  }
  Layout();

  // The show_disable_cookie_blocking_ui_ state has a different title
  // configuration. To avoid jumping UI, don't resize the bubble. This should be
  // safe as the bubble in this state has less content than in Enabled state.
  if (!show_disable_cookie_blocking_ui_)
    SizeToContents();
}

void CookieControlsBubbleView::CloseBubble() {
  // Widget's Close() is async, but we don't want to use cookie_bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  g_instance = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

int CookieControlsBubbleView::GetDialogButtons() const {
  if (show_disable_cookie_blocking_ui_ ||
      status_ == CookieControlsController::Status::kDisabledForSite) {
    return ui::DIALOG_BUTTON_OK;
  }
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 CookieControlsBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (show_disable_cookie_blocking_ui_)
    return l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TURN_OFF_BUTTON);
  DCHECK_EQ(status_, CookieControlsController::Status::kDisabledForSite);
  return l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TURN_ON_BUTTON);
}

void CookieControlsBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto text = std::make_unique<views::Label>(base::string16(),
                                             views::style::CONTEXT_LABEL);
  text->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text->SetMultiLine(true);
  text_ = AddChildView(std::move(text));
}

std::unique_ptr<views::View> CookieControlsBubbleView::CreateExtraView() {
  auto view = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_NOT_WORKING_TITLE));
  not_working_link_ = view.get();
  not_working_link_->set_listener(this);
  return std::move(view);
}

void CookieControlsBubbleView::AddedToWidget() {
  auto header_view = std::make_unique<NonAccessibleImageView>();
  header_view_ = header_view.get();
  GetBubbleFrameView()->SetHeaderView(std::move(header_view));
}

gfx::Size CookieControlsBubbleView::CalculatePreferredSize() const {
  // The total width of this view should always be identical to the width
  // of the header images.
  int width = ui::ResourceBundle::GetSharedInstance()
                  .GetImageSkiaNamed(IDR_COOKIE_BLOCKING_ON_HEADER)
                  ->width();
  return gfx::Size{width, GetHeightForWidth(width)};
}

base::string16 CookieControlsBubbleView::GetWindowTitle() const {
  if (show_disable_cookie_blocking_ui_)
    return l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_NOT_WORKING_TITLE);
  switch (status_) {
    case CookieControlsController::Status::kEnabled:
      return l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_DIALOG_TITLE);
    case CookieControlsController::Status::kDisabledForSite:
      return l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_DIALOG_TITLE_OFF);
    case CookieControlsController::Status::kUninitialized:
      return base::string16();
    case CookieControlsController::Status::kDisabled:
      NOTREACHED();
      return base::string16();
  }
}

bool CookieControlsBubbleView::ShouldShowWindowTitle() const {
  return true;
}

bool CookieControlsBubbleView::ShouldShowCloseButton() const {
  return true;
}

void CookieControlsBubbleView::WindowClosing() {
  // |cookie_bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  bool this_bubble = g_instance == this;
  if (this_bubble)
    g_instance = nullptr;

  controller_->OnBubbleUiClosing(web_contents());
}

bool CookieControlsBubbleView::Accept() {
  if (show_disable_cookie_blocking_ui_) {
    controller_->OnCookieBlockingEnabledForSite(false);
  } else {
    DCHECK_EQ(status_, CookieControlsController::Status::kDisabledForSite);
    controller_->OnCookieBlockingEnabledForSite(true);
  }
  return false;
}

bool CookieControlsBubbleView::Close() {
  return Cancel();
}

void CookieControlsBubbleView::LinkClicked(views::Link* source,
                                           int event_flags) {
  DCHECK_EQ(source, not_working_link_);
  DCHECK_EQ(status_, CookieControlsController::Status::kEnabled);
  // Don't go through the controller as this is an intermediary state that
  // is only relevant for the bubble UI.
  show_disable_cookie_blocking_ui_ = true;
  UpdateUi();
}
