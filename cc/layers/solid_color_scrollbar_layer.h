// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_
#define CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_interface.h"

namespace cc {

class CC_EXPORT SolidColorScrollbarLayer : public ScrollbarLayerInterface,
                                           public Layer {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  static scoped_refptr<SolidColorScrollbarLayer> Create(
      ScrollbarOrientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);

  SolidColorScrollbarLayer(const SolidColorScrollbarLayer&) = delete;
  SolidColorScrollbarLayer& operator=(const SolidColorScrollbarLayer&) = delete;

  // Layer overrides.
  bool OpacityCanAnimateOnImplThread() const override;

  void SetOpacity(float opacity) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  void SetNeedsDisplayRect(const gfx::Rect& rect) override;

  bool HitTestable() const override;

  // ScrollbarLayerInterface
  void SetScrollElementId(ElementId element_id) override;

 protected:
  SolidColorScrollbarLayer(ScrollbarOrientation orientation,
                           int thumb_thickness,
                           int track_start,
                           bool is_left_side_vertical_scrollbar);
  ~SolidColorScrollbarLayer() override;

  ElementId scroll_element_id_;
  ScrollbarOrientation orientation_;
  int thumb_thickness_;
  int track_start_;
  bool is_left_side_vertical_scrollbar_;
};

}  // namespace cc

#endif  // CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_
