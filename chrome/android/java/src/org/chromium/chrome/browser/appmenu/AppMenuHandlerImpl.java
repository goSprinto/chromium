// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.widget.PopupMenu;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ObservableSupplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;

import java.util.ArrayList;
import java.util.List;

/**
 * Object responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions.
 */
class AppMenuHandlerImpl
        implements AppMenuHandler, StartStopWithNativeObserver, ConfigurationChangedObserver {
    private AppMenu mAppMenu;
    private AppMenuDragHelper mAppMenuDragHelper;
    private Menu mMenu;
    private final List<AppMenuBlocker> mBlockers;
    private final List<AppMenuObserver> mObservers;
    private final int mMenuResourceId;
    private final View mHardwareButtonMenuAnchor;

    private final AppMenuPropertiesDelegate mDelegate;
    private final AppMenuDelegate mAppMenuDelegate;
    private final View mDecorView;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final @Nullable ObservableSupplier<OverviewModeBehavior> mOverviewModeBehaviorSupplier;
    private @Nullable Callback<OverviewModeBehavior> mOverviewModeSupplierCallback;
    private @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;

    /**
     * The resource id of the menu item to highlight when the menu next opens. A value of
     * {@code null} means no item will be highlighted.  This value will be cleared after the menu is
     * opened.
     */
    private Integer mHighlightMenuId;

    /**
     *  Whether the highlighted item should use a circle highlight or not.
     */
    private boolean mCircleHighlight;

    /**
     * Constructs an AppMenuHandlerImpl object.
     * @param delegate Delegate used to check the desired AppMenu properties on show.
     * @param appMenuDelegate The AppMenuDelegate to handle menu item selection.
     * @param menuResourceId Resource Id that should be used as the source for the menu items.
     *            It is assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *            activity.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *            activity.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *            {@link OverviewModeBehavior} associated with the containing activity.
     */
    public AppMenuHandlerImpl(AppMenuPropertiesDelegate delegate, AppMenuDelegate appMenuDelegate,
            int menuResourceId, View decorView,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @Nullable ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        mAppMenuDelegate = appMenuDelegate;
        mDelegate = delegate;
        mDecorView = decorView;
        mBlockers = new ArrayList<>();
        mObservers = new ArrayList<>();
        mMenuResourceId = menuResourceId;
        mHardwareButtonMenuAnchor = mDecorView.findViewById(R.id.menu_anchor_stub);

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            // OverviewModeBehavior.OverviewModeObserver implementation
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                hideAppMenu();
            }

            @Override
            public void onOverviewModeFinishedShowing() {
                // Ideally we wouldn't allow the app menu to show while animating the overview mode.
                // This is hard to track, however, because in some instances
                // #onOverviewModeStartedShowing is called after #onOverviewModeFinishedShowing (see
                // https://crbug.com/969047). Once that bug is fixed, we can remove this call to
                // hide in favor of disallowing app menu shows during animation. Alternatively, we
                // could expose a way to query whether an animation is in progress.
                hideAppMenu();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                hideAppMenu();
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                hideAppMenu();
            }
        };

        // TODO(twellington): Move overview mode/app menu logic to parent root UI coordinator (to
        // control interaction between peer components)?
        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;
        if (mOverviewModeBehaviorSupplier != null) {
            mOverviewModeSupplierCallback = overviewModeBehavior -> {
                if (mOverviewModeBehavior != null) {
                    mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
                }

                mOverviewModeBehavior = overviewModeBehavior;
                mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
            };
            mOverviewModeBehaviorSupplier.addObserver(mOverviewModeSupplierCallback);
        }

        assert mHardwareButtonMenuAnchor
                != null : "Using AppMenu requires to have menu_anchor_stub view";
    }

    /**
     * Called when the containing activity is being destroyed.
     */
    void destroy() {
        // Prevent the menu window from leaking.
        hideAppMenu();

        mActivityLifecycleDispatcher.unregister(this);
        if (mOverviewModeBehaviorSupplier != null) {
            mOverviewModeBehaviorSupplier.removeObserver(mOverviewModeSupplierCallback);
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
    }

    @Override
    public void menuItemContentChanged(int menuRowId) {
        if (mAppMenu != null) mAppMenu.menuItemContentChanged(menuRowId);
    }

    @Override
    public void clearMenuHighlight() {
        setMenuHighlight(null, false);
    }

    @Override
    public void setMenuHighlight(Integer highlightItemId, boolean circleHighlight) {
        if (mHighlightMenuId == null && highlightItemId == null) return;
        if (mHighlightMenuId != null && mHighlightMenuId.equals(highlightItemId)) return;
        mHighlightMenuId = highlightItemId;
        mCircleHighlight = circleHighlight;
        boolean highlighting = mHighlightMenuId != null;
        for (AppMenuObserver observer : mObservers) observer.onMenuHighlightChanged(highlighting);
    }

    /**
     * Show the app menu.
     * @param anchorView    Anchor view (usually a menu button) to be used for the popup, if null is
     *                      passed then hardware menu button anchor will be used.
     * @param startDragging Whether dragging is started. For example, if the app menu is showed by
     *                      tapping on a button, this should be false. If it is showed by start
     *                      dragging down on the menu button, this should be true. Note that if
     *                      anchorView is null, this must be false since we no longer support
     *                      hardware menu button dragging.
     * @param showFromBottom Whether the menu should be shown from the bottom up.
     * @return              True, if the menu is shown, false, if menu is not shown, example
     *                      reasons: the menu is not yet available to be shown, or the menu is
     *                      already showing.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("ResourceType")
    boolean showAppMenu(View anchorView, boolean startDragging, boolean showFromBottom) {
        if (!shouldShowAppMenu() || isAppMenuShowing()) return false;

        TextBubble.dismissBubbles();
        boolean isByPermanentButton = false;

        Context context = mDecorView.getContext();
        WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        int rotation = wm.getDefaultDisplay().getRotation();
        if (anchorView == null) {
            // This fixes the bug where the bottom of the menu starts at the top of
            // the keyboard, instead of overlapping the keyboard as it should.
            int displayHeight = context.getResources().getDisplayMetrics().heightPixels;
            Rect rect = new Rect();
            mDecorView.getWindowVisibleDisplayFrame(rect);
            int statusBarHeight = rect.top;
            mHardwareButtonMenuAnchor.setY((displayHeight - statusBarHeight));

            anchorView = mHardwareButtonMenuAnchor;
            isByPermanentButton = true;
        }

        assert !(isByPermanentButton && startDragging);

        if (mMenu == null) {
            // Use a PopupMenu to create the Menu object. Note this is not the same as the
            // AppMenu (mAppMenu) created below.
            PopupMenu tempMenu = new PopupMenu(context, anchorView);
            tempMenu.inflate(mMenuResourceId);
            mMenu = tempMenu.getMenu();
        }
        mDelegate.prepareMenu(mMenu);

        ContextThemeWrapper wrapper =
                new ContextThemeWrapper(context, R.style.OverflowMenuThemeOverlay);

        if (mAppMenu == null) {
            TypedArray a = wrapper.obtainStyledAttributes(new int[] {
                    android.R.attr.listPreferredItemHeightSmall, android.R.attr.listDivider});
            int itemRowHeight = a.getDimensionPixelSize(0, 0);
            Drawable itemDivider = a.getDrawable(1);
            int itemDividerHeight = itemDivider != null ? itemDivider.getIntrinsicHeight() : 0;
            a.recycle();
            mAppMenu = new AppMenu(
                    mMenu, itemRowHeight, itemDividerHeight, this, context.getResources());
            mAppMenuDragHelper = new AppMenuDragHelper(context, mAppMenu, itemRowHeight);
        }

        // Get the height and width of the display.
        Rect appRect = new Rect();
        mDecorView.getWindowVisibleDisplayFrame(appRect);

        // Use full size of window for abnormal appRect.
        if (appRect.left < 0 && appRect.top < 0) {
            appRect.left = 0;
            appRect.top = 0;
            appRect.right = mDecorView.getWidth();
            appRect.bottom = mDecorView.getHeight();
        }
        Point pt = new Point();
        wm.getDefaultDisplay().getSize(pt);

        int footerResourceId = 0;
        if (mDelegate.shouldShowFooter(appRect.height())) {
            footerResourceId = mDelegate.getFooterResourceId();
        }
        int headerResourceId = 0;
        if (mDelegate.shouldShowHeader(appRect.height())) {
            headerResourceId = mDelegate.getHeaderResourceId();
        }
        mAppMenu.show(wrapper, anchorView, isByPermanentButton, rotation, appRect, pt.y,
                footerResourceId, headerResourceId, mHighlightMenuId, mCircleHighlight,
                showFromBottom);
        mAppMenuDragHelper.onShow(startDragging);
        clearMenuHighlight();
        RecordUserAction.record("MobileMenuShow");
        return true;
    }

    void appMenuDismissed() {
        mAppMenuDragHelper.finishDragging();
    }

    @Override
    public boolean isAppMenuShowing() {
        return mAppMenu != null && mAppMenu.isShowing();
    }

    /**
     * @return The App Menu that the menu handler is interacting with.
     */
    public AppMenu getAppMenu() {
        return mAppMenu;
    }

    AppMenuDragHelper getAppMenuDragHelper() {
        return mAppMenuDragHelper;
    }

    @Override
    public void hideAppMenu() {
        if (mAppMenu != null && mAppMenu.isShowing()) mAppMenu.dismiss();
    }

    @Override
    public AppMenuButtonHelper createAppMenuButtonHelper() {
        return new AppMenuButtonHelperImpl(this);
    }

    @Override
    public void addObserver(AppMenuObserver observer) {
        mObservers.add(observer);
    }

    @Override
    public void removeObserver(AppMenuObserver observer) {
        mObservers.remove(observer);
    }

    // StartStopWithNativeObserver implementation
    @Override
    public void onStartWithNative() {}

    @Override
    public void onStopWithNative() {
        hideAppMenu();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        hideAppMenu();
    }

    @VisibleForTesting
    public void onOptionsItemSelected(MenuItem item) {
        mAppMenuDelegate.onOptionsItemSelected(
                item.getItemId(), mDelegate.getBundleForMenuItem(item));
    }

    /**
     * Called by AppMenu to report that the App Menu visibility has changed.
     * @param isVisible Whether the App Menu is showing.
     */
    void onMenuVisibilityChanged(boolean isVisible) {
        for (int i = 0; i < mObservers.size(); ++i) {
            mObservers.get(i).onMenuVisibilityChanged(isVisible);
        }
    }

    /**
     * A notification that the header view has been inflated.
     * @param view The inflated view.
     */
    void onHeaderViewInflated(View view) {
        if (mDelegate != null) mDelegate.onHeaderViewInflated(this, view);
    }

    /**
     * A notification that the footer view has been inflated.
     * @param view The inflated view.
     */
    void onFooterViewInflated(View view) {
        if (mDelegate != null) mDelegate.onFooterViewInflated(this, view);
    }

    /**
     * Registers an {@link AppMenuBlocker} used to help determine whether the app menu can be shown.
     * @param blocker An {@link AppMenuBlocker} to check before attempting to show the app menu.
     */
    void registerAppMenuBlocker(AppMenuBlocker blocker) {
        if (!mBlockers.contains(blocker)) mBlockers.add(blocker);
    }

    /**
     * @param blocker The {@link AppMenuBlocker} to unregister.
     */
    void unregisterAppMenuBlocker(AppMenuBlocker blocker) {
        mBlockers.remove(blocker);
    }

    boolean shouldShowAppMenu() {
        for (int i = 0; i < mBlockers.size(); i++) {
            if (!mBlockers.get(i).canShowAppMenu()) return false;
        }
        return true;
    }
}
