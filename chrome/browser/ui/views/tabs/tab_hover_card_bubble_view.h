// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
class Widget;
}  // namespace views

class Tab;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView,
                               public ThumbnailImage::Observer {
 public:
  explicit TabHoverCardBubbleView(Tab* tab);

  ~TabHoverCardBubbleView() override;

  // Updates card content and anchoring and shows the tab hover card.
  void UpdateAndShow(Tab* tab);

  void UpdateAnchorBounds(gfx::Rect anchor_bounds);

  void FadeOutToHide();

  bool IsFadingOut() const;

  // Returns the target tab (if any).
  views::View* GetDesiredAnchorView();

  // Record a histogram metric of tab hover cards seen prior to a tab being
  // selected by mouse press.
  void RecordHoverCardsSeenRatioMetric();

  void reset_hover_cards_seen_count() { hover_cards_seen_count_ = 0; }

  // BubbleDialogDelegateView:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  ax::mojom::Role GetAccessibleWindowRole() override;
  int GetDialogButtons() const override;

  void set_last_mouse_exit_timestamp(
      base::TimeTicks last_mouse_exit_timestamp) {
    last_mouse_exit_timestamp_ = last_mouse_exit_timestamp;
  }

 private:
  friend class TabHoverCardBubbleViewBrowserTest;
  friend class TabHoverCardBubbleViewInteractiveUiTest;
  class WidgetFadeAnimationDelegate;
  class WidgetSlideAnimationDelegate;

  // Get delay in milliseconds based on tab width.
  base::TimeDelta GetDelay(int tab_width) const;

  void FadeInToShow();

  // Updates and formats title, alert state, domain, and preview image.
  void UpdateCardContent(const Tab* tab);

  void RegisterToThumbnailImageUpdates(
      scoped_refptr<ThumbnailImage> thumbnail_image);

  void ClearPreviewImage();

  // ThumbnailImage::Observer:
  void OnThumbnailImageAvailable(gfx::ImageSkia thumbnail_image) override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  void RecordTimeSinceLastSeenMetric(base::TimeDelta elapsed_time);

  base::OneShotTimer delayed_show_timer_;

  // Fade animations interfere with browser tests so we disable them in tests.
  static bool disable_animations_for_testing_;
  std::unique_ptr<WidgetFadeAnimationDelegate> fade_animation_delegate_;
  // Used to animate the tab hover card's movement between tabs.
  std::unique_ptr<WidgetSlideAnimationDelegate> slide_animation_delegate_;

  // Timestamp of the last time a hover card was visible, recorded before it is
  // hidden. This is used for metrics.
  base::TimeTicks last_visible_timestamp_;

  // Timestamp of the last time the hover card is hidden by the mouse leaving
  // the tab strip. This is used for reshowing the hover card without delay if
  // the mouse reenters within a given amount of time.
  base::TimeTicks last_mouse_exit_timestamp_;

  views::Widget* widget_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::Label* alert_state_label_ = nullptr;
  views::Label* domain_label_ = nullptr;
  views::ImageView* preview_image_ = nullptr;

  // Counter used to keey track of the number of tab hover cards seen before a
  // tab is selected by mouse press.
  size_t hover_cards_seen_count_ = 0;
  scoped_refptr<ThumbnailImage> thumbnail_image_;
  ScopedObserver<ThumbnailImage, ThumbnailImage::Observer> thumbnail_observer_{
      this};

  base::WeakPtrFactory<TabHoverCardBubbleView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabHoverCardBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
