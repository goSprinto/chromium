// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mojom/geometry_traits_test_service.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/quaternion.h"

namespace gfx {

namespace {

class GeometryStructTraitsTest : public testing::Test,
                                 public mojom::GeometryTraitsTestService {
 public:
  GeometryStructTraitsTest() {}

 protected:
  mojo::Remote<mojom::GeometryTraitsTestService> GetTraitsTestProxy() {
    mojo::Remote<mojom::GeometryTraitsTestService> proxy;
    traits_test_receivers_.Add(this, proxy.BindNewPipeAndPassReceiver());
    return proxy;
  }

 private:
  // GeometryTraitsTestService:
  void EchoPoint(const Point& p, EchoPointCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoPointF(const PointF& p, EchoPointFCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoPoint3F(const Point3F& p, EchoPoint3FCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoSize(const Size& s, EchoSizeCallback callback) override {
    std::move(callback).Run(s);
  }

  void EchoSizeF(const SizeF& s, EchoSizeFCallback callback) override {
    std::move(callback).Run(s);
  }

  void EchoRect(const Rect& r, EchoRectCallback callback) override {
    std::move(callback).Run(r);
  }

  void EchoRectF(const RectF& r, EchoRectFCallback callback) override {
    std::move(callback).Run(r);
  }

  void EchoInsets(const Insets& i, EchoInsetsCallback callback) override {
    std::move(callback).Run(i);
  }

  void EchoInsetsF(const InsetsF& i, EchoInsetsFCallback callback) override {
    std::move(callback).Run(i);
  }

  void EchoVector2d(const Vector2d& v, EchoVector2dCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoVector2dF(const Vector2dF& v,
                     EchoVector2dFCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoVector3dF(const Vector3dF& v,
                     EchoVector3dFCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoQuaternion(const Quaternion& q,
                      EchoQuaternionCallback callback) override {
    std::move(callback).Run(q);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<GeometryTraitsTestService> traits_test_receivers_;

  DISALLOW_COPY_AND_ASSIGN(GeometryStructTraitsTest);
};

}  // namespace

TEST_F(GeometryStructTraitsTest, Point) {
  const int32_t x = 1234;
  const int32_t y = -5678;
  gfx::Point input(x, y);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Point output;
  proxy->EchoPoint(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST_F(GeometryStructTraitsTest, PointF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  gfx::PointF input(x, y);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::PointF output;
  proxy->EchoPointF(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST_F(GeometryStructTraitsTest, Point3F) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  const float z = 5432.1f;
  gfx::Point3F input(x, y, z);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Point3F output;
  proxy->EchoPoint3F(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(z, output.z());
}

TEST_F(GeometryStructTraitsTest, Size) {
  const int32_t width = 1234;
  const int32_t height = 5678;
  gfx::Size input(width, height);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Size output;
  proxy->EchoSize(input, &output);
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, SizeF) {
  const float width = 1234.5f;
  const float height = 6789.6f;
  gfx::SizeF input(width, height);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::SizeF output;
  proxy->EchoSizeF(input, &output);
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, Rect) {
  const int32_t x = 1234;
  const int32_t y = 5678;
  const int32_t width = 4321;
  const int32_t height = 8765;
  gfx::Rect input(x, y, width, height);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Rect output;
  proxy->EchoRect(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, RectF) {
  const float x = 1234.1f;
  const float y = 5678.2f;
  const float width = 4321.3f;
  const float height = 8765.4f;
  gfx::RectF input(x, y, width, height);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::RectF output;
  proxy->EchoRectF(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, Insets) {
  const int32_t top = 1234;
  const int32_t left = 5678;
  const int32_t bottom = 4321;
  const int32_t right = 8765;
  gfx::Insets input(top, left, bottom, right);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Insets output;
  proxy->EchoInsets(input, &output);
  EXPECT_EQ(top, output.top());
  EXPECT_EQ(left, output.left());
  EXPECT_EQ(bottom, output.bottom());
  EXPECT_EQ(right, output.right());
}

TEST_F(GeometryStructTraitsTest, InsetsF) {
  const float top = 1234.1f;
  const float left = 5678.2f;
  const float bottom = 4321.3f;
  const float right = 8765.4f;
  gfx::InsetsF input(top, left, bottom, right);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::InsetsF output;
  proxy->EchoInsetsF(input, &output);
  EXPECT_EQ(top, output.top());
  EXPECT_EQ(left, output.left());
  EXPECT_EQ(bottom, output.bottom());
  EXPECT_EQ(right, output.right());
}

TEST_F(GeometryStructTraitsTest, Vector2d) {
  const int32_t x = 1234;
  const int32_t y = -5678;
  gfx::Vector2d input(x, y);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Vector2d output;
  proxy->EchoVector2d(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST_F(GeometryStructTraitsTest, Vector2dF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  gfx::Vector2dF input(x, y);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Vector2dF output;
  proxy->EchoVector2dF(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST_F(GeometryStructTraitsTest, Vector3dF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  const float z = 5432.1f;
  gfx::Vector3dF input(x, y, z);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Vector3dF output;
  proxy->EchoVector3dF(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(z, output.z());
}

TEST_F(GeometryStructTraitsTest, Quaternion) {
  const double x = 1234.5;
  const double y = 6789.6;
  const double z = 31415.9;
  const double w = 27182.8;
  gfx::Quaternion input(x, y, z, w);
  mojo::Remote<mojom::GeometryTraitsTestService> proxy = GetTraitsTestProxy();
  gfx::Quaternion output;
  proxy->EchoQuaternion(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(z, output.z());
  EXPECT_EQ(w, output.w());
}

}  // namespace gfx
