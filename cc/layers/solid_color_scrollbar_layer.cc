// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include <memory>

#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"

namespace cc {

std::unique_ptr<LayerImpl> SolidColorScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  const bool kIsOverlayScrollbar = true;
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), orientation_, thumb_thickness_, track_start_,
      is_left_side_vertical_scrollbar_, kIsOverlayScrollbar);
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar) {
  return base::WrapRefCounted(
      new SolidColorScrollbarLayer(orientation, thumb_thickness, track_start,
                                   is_left_side_vertical_scrollbar));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar)
    : orientation_(orientation),
      thumb_thickness_(thumb_thickness),
      track_start_(track_start),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar) {
  Layer::SetOpacity(0.f);
  SetIsScrollbar(true);
}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() = default;

void SolidColorScrollbarLayer::SetOpacity(float opacity) {
  // The opacity of a solid color scrollbar layer is always 0 on main thread.
  DCHECK_EQ(opacity, 0.f);
  Layer::SetOpacity(opacity);
}

void SolidColorScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  SolidColorScrollbarLayerImpl* scrollbar_layer =
      static_cast<SolidColorScrollbarLayerImpl*>(layer);

  DCHECK(!scrollbar_layer->HitTestable());

  scrollbar_layer->SetScrollElementId(scroll_element_id_);
}

void SolidColorScrollbarLayer::SetNeedsDisplayRect(const gfx::Rect& rect) {
  // Never needs repaint.
}

bool SolidColorScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

void SolidColorScrollbarLayer::SetScrollElementId(ElementId element_id) {
  if (element_id == scroll_element_id_)
    return;

  scroll_element_id_ = element_id;
  SetNeedsCommit();
}

bool SolidColorScrollbarLayer::HitTestable() const {
  // Android scrollbars can't be interacted with by user input. They should
  // avoid hit testing so we don't enter any scrollbar scrolling code paths.
  return false;
}

}  // namespace cc
