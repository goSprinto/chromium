// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/gurl.h"

using security_state::SafetyTipStatus;

SafetyTipPageInfoBubbleView::SafetyTipPageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& url,
    const GURL& suggested_url,
    base::OnceCallback<void(safety_tips::SafetyTipInteraction)> close_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_SAFETY_TIP,
                             web_contents),
      safety_tip_status_(safety_tip_status),
      url_(url),
      suggested_url_(suggested_url),
      close_callback_(std::move(close_callback)) {
  // Keep the bubble open until explicitly closed (or we navigate away, a tab is
  // created over it, etc).
  set_close_on_deactivate(false);

  const base::string16 title_text =
      safety_tips::GetSafetyTipTitle(safety_tip_status, suggested_url);
  set_window_title(title_text);

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Replace the original title view with our formatted title.
  views::Label* original_title =
      static_cast<views::Label*>(GetBubbleFrameView()->title());
  views::StyledLabel::RangeStyleInfo name_style;
  const auto kSizeDeltaInPixels = 3;
  name_style.custom_font = original_title->GetDefaultFontList().Derive(
      kSizeDeltaInPixels, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::BOLD);
  views::StyledLabel::RangeStyleInfo base_style;
  base_style.custom_font = original_title->GetDefaultFontList().Derive(
      kSizeDeltaInPixels, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::NORMAL);

  auto new_title = std::make_unique<views::StyledLabel>(title_text, nullptr);
  new_title->AddStyleRange(gfx::Range(0, title_text.length()), name_style);
  GetBubbleFrameView()->SetTitleView(std::move(new_title));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  gfx::Insets insets =
      layout_provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);
  set_margins(gfx::Insets(0, 0, insets.bottom(), 0));

  // Configure layout.
  views::GridLayout* bubble_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnId = 0;
  views::ColumnSet* bubble_col_set = bubble_layout->AddColumnSet(kColumnId);
  bubble_col_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                            1.0, views::GridLayout::USE_PREF, 0, 0);

  // TODO(crbug/996731): Add banner once available. See crrev/c/1816805/7.

  auto bottom_view = std::make_unique<views::View>();
  views::GridLayout* bottom_layout =
      bottom_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* bottom_column_set = bottom_layout->AddColumnSet(0);
  bottom_column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      insets.left());
  bottom_column_set->AddColumn(views::GridLayout::LEADING,
                               views::GridLayout::FILL, 1.0,
                               views::GridLayout::USE_PREF, 0, 0);
  bottom_column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      insets.right());

  // Add text description.
  const int spacing =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  bottom_layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                                     views::GridLayout::kFixedSize, spacing);
  auto text_label = std::make_unique<views::Label>(
      safety_tips::GetSafetyTipDescription(safety_tip_status, suggested_url_));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(
      layout_provider->GetDistanceMetric(DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      insets.left() - insets.right());
  bottom_layout->AddView(std::move(text_label));

  // Add buttons.
  // To make the rest of the layout simpler, they live in their own grid layout.
  auto button_view = std::make_unique<views::View>();
  views::GridLayout* button_layout =
      button_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* button_column_set = button_layout->AddColumnSet(0);
  button_column_set->AddColumn(views::GridLayout::LEADING,
                               views::GridLayout::CENTER, 0.0,
                               views::GridLayout::USE_PREF, 0, 0);
  button_column_set->AddPaddingColumn(1.f, 1);
  button_column_set->AddColumn(views::GridLayout::TRAILING,
                               views::GridLayout::FILL, 0.0,
                               views::GridLayout::USE_PREF, 0, 0);
  button_column_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  button_column_set->AddColumn(views::GridLayout::TRAILING,
                               views::GridLayout::FILL, 0.0,
                               views::GridLayout::USE_PREF, 0, 0);

  button_layout->StartRow(views::GridLayout::kFixedSize, kColumnId);

  // More info button.
  auto info_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_MORE_INFO_LINK);
  auto info_link = std::make_unique<views::StyledLabel>(info_text, this);
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  gfx::Range details_range(0, info_text.length());
  info_link->AddStyleRange(details_range, link_style);
  info_link->SizeToFit(0);
  info_button_ = button_layout->AddView(std::move(info_link));

  // Ignore button.
  std::unique_ptr<views::Button> ignore_button(
      views::MdTextButton::CreateSecondaryUiButton(
          this,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_IGNORE_BUTTON)));
  ignore_button->SetID(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_IGNORE_WARNING);
  ignore_button_ = button_layout->AddView(std::move(ignore_button));

  // Leave site button.
  std::unique_ptr<views::Button> leave_button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this,
          l10n_util::GetStringUTF16(
              safety_tips::GetSafetyTipLeaveButtonId(safety_tip_status))));
  leave_button->SetID(PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_LEAVE_SITE);
  leave_button_ = button_layout->AddView(std::move(leave_button));

  bottom_layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                                     views::GridLayout::kFixedSize, spacing);
  bottom_layout->AddView(
      std::move(button_view), 1, 1, views::GridLayout::LEADING,
      views::GridLayout::LEADING,
      layout_provider->GetDistanceMetric(DISTANCE_BUBBLE_PREFERRED_WIDTH) -
          insets.left() - insets.right(),
      0);
  bubble_layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  bubble_layout->AddView(std::move(bottom_view));

  Layout();
  SizeToContents();
}

SafetyTipPageInfoBubbleView::~SafetyTipPageInfoBubbleView() {}

void SafetyTipPageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  bool should_set_ignore = false;

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kLostFocus:
      // We require that the user explicitly interact with the bubble, so do
      // nothing in these cases.
      break;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      // If they've left the site, we can still ignore the result; if they
      // stumble there again, we should warn again.
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
      action_taken_ = safety_tips::SafetyTipInteraction::kDismissWithEsc;
      should_set_ignore = true;
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      action_taken_ = safety_tips::SafetyTipInteraction::kDismissWithClose;
      should_set_ignore = true;
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      action_taken_ = safety_tips::SafetyTipInteraction::kDismissWithIgnore;
      should_set_ignore = true;
      break;
  }
  std::move(close_callback_).Run(action_taken_);
  if (should_set_ignore) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (browser) {
      safety_tips::ReputationService::Get(browser->profile())
          ->SetUserIgnore(web_contents(), url_, action_taken_);
    }
  }
}

void SafetyTipPageInfoBubbleView::ButtonPressed(views::Button* button,
                                                const ui::Event& event) {
  switch (button->GetID()) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_LEAVE_SITE:
      action_taken_ = safety_tips::SafetyTipInteraction::kLeaveSite;
      safety_tips::LeaveSite(web_contents(),
                             safety_tip_status_ == SafetyTipStatus::kLookalike
                                 ? suggested_url_
                                 : GURL(safety_tips::kSafeUrl));
      return;

    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_IGNORE_WARNING:
      action_taken_ = safety_tips::SafetyTipInteraction::kDismiss;
      GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kCancelButtonClicked);
      return;
  }
  NOTREACHED();
}

void SafetyTipPageInfoBubbleView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  action_taken_ = safety_tips::SafetyTipInteraction::kLearnMore;
  safety_tips::OpenHelpCenter(web_contents());
}

namespace safety_tips {

void ShowSafetyTipDialog(
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& virtual_url,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(
          browser, bubble_anchor_util::kLocationBar);
  gfx::Rect anchor_rect =
      configuration.anchor_view
          ? gfx::Rect()
          : bubble_anchor_util::GetPageInfoAnchorRect(browser);
  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();
  gfx::NativeView parent_view = platform_util::GetViewForWindow(parent_window);

  views::BubbleDialogDelegateView* bubble = new SafetyTipPageInfoBubbleView(
      configuration.anchor_view, anchor_rect, parent_view, web_contents,
      safety_tip_status, virtual_url, suggested_url, std::move(close_callback));

  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}

}  // namespace safety_tips

PageInfoBubbleViewBase* CreateSafetyTipBubbleForTesting(
    gfx::NativeView parent_view,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& virtual_url,
    const GURL& suggested_url,
    base::OnceCallback<void(safety_tips::SafetyTipInteraction)>
        close_callback) {
  return new SafetyTipPageInfoBubbleView(
      nullptr, gfx::Rect(), parent_view, web_contents, safety_tip_status,
      virtual_url, suggested_url, std::move(close_callback));
}
