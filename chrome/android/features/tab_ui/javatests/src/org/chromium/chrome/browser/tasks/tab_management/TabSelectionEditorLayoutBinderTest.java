// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.support.annotation.NonNull;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.HashSet;
import java.util.concurrent.atomic.AtomicBoolean;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

/**
 * Tests for {@link TabSelectionEditorLayoutBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabSelectionEditorLayoutBinderTest extends DummyUiActivityTestCase {
    private TabSelectionEditorLayout mEditorLayoutView;
    private PropertyModel mModel = new PropertyModel(TabSelectionEditorProperties.ALL_KEYS);
    private PropertyModelChangeProcessor mMCP;
    private SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new LinearLayout(getActivity());
        TabSelectionEditorLayout.TabSelectionEditorLayoutPositionProvider positionProvider =
                () -> null;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);
            mEditorLayoutView =
                    (TabSelectionEditorLayout) getActivity().getLayoutInflater().inflate(
                            R.layout.tab_selection_editor_layout, null);
            mEditorLayoutView.initialize(view, null, new RecyclerView.Adapter() {
                @NonNull
                @Override
                public RecyclerView.ViewHolder onCreateViewHolder(
                        @NonNull ViewGroup viewGroup, int i) {
                    return null;
                }

                @Override
                public void onBindViewHolder(@NonNull RecyclerView.ViewHolder viewHolder, int i) {}

                @Override
                public int getItemCount() {
                    return 0;
                }
            }, mSelectionDelegate, positionProvider);
        });
        mMCP = PropertyModelChangeProcessor.create(
                mModel, mEditorLayoutView, TabSelectionEditorLayoutBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testBindViews() {
        // TODO(1005929): test other properties as well.
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_TEXT, "Test");
        Assert.assertEquals("Test",
                ((TextView) mEditorLayoutView.findViewById(R.id.action_button))
                        .getText()
                        .toString());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testBindActionButtonClickListener() {
        AtomicBoolean actionButtonClicked = new AtomicBoolean(false);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_LISTENER,
                v -> { actionButtonClicked.set(true); });
        mEditorLayoutView.findViewById(R.id.action_button).performClick();
        assertTrue(actionButtonClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testActionButtonEnabling() {
        Button button = mEditorLayoutView.findViewById(R.id.action_button);
        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, 1);
        assertFalse(button.isEnabled());

        HashSet<Integer> selectedItem = new HashSet<>(Arrays.asList(1));
        mSelectionDelegate.setSelectedItems(selectedItem);
        assertTrue(button.isEnabled());

        mModel.set(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD, 2);
        mSelectionDelegate.setSelectedItems(selectedItem);
        assertFalse(button.isEnabled());
    }
}