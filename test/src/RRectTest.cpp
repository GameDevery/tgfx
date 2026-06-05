/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "tgfx/core/RRect.h"
#include "utils/TestUtils.h"

namespace tgfx {

TGFX_TEST(RRectTest, SetRectXY) {
  RRect rRect = {};
  rRect.setRectXY(Rect::MakeWH(100, 80), 10, 15);
  EXPECT_EQ(rRect.type(), RRect::Type::Simple);
  EXPECT_EQ(rRect.radii()[0], (Point{10, 15}));
  EXPECT_EQ(rRect.radii()[1], (Point{10, 15}));
  EXPECT_EQ(rRect.radii()[2], (Point{10, 15}));
  EXPECT_EQ(rRect.radii()[3], (Point{10, 15}));

  // Zero radii -> Rect type.
  rRect.setRectXY(Rect::MakeWH(100, 80), 0, 0);
  EXPECT_EQ(rRect.type(), RRect::Type::Rect);

  // Radii filling the rect -> Oval type.
  rRect.setRectXY(Rect::MakeWH(100, 80), 50, 40);
  EXPECT_EQ(rRect.type(), RRect::Type::Oval);

  // Radii exceeding rect size -> clamped proportionally. The clamped x radius (40) is less than
  // halfWidth (50), so the result is still Simple rather than Oval.
  rRect.setRectXY(Rect::MakeWH(100, 80), 200, 200);
  EXPECT_EQ(rRect.type(), RRect::Type::Simple);
  EXPECT_FLOAT_EQ(rRect.radii()[0].x, 40.f);
  EXPECT_FLOAT_EQ(rRect.radii()[0].y, 40.f);
}

TGFX_TEST(RRectTest, SetOval) {
  auto rRect = RRect::MakeOval(Rect::MakeWH(120, 80));
  EXPECT_EQ(rRect.type(), RRect::Type::Oval);
  EXPECT_EQ(rRect.radii()[0], (Point{60, 40}));
  EXPECT_EQ(rRect.radii()[2], (Point{60, 40}));
}

TGFX_TEST(RRectTest, SetRectRadii_Complex) {
  // Different corners -> Complex type.
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 20}, {10, 10}, {5, 5}, {30, 30}}});
  EXPECT_EQ(rRect.type(), RRect::Type::Complex);
  EXPECT_EQ(rRect.radii()[0], (Point{20, 20}));
  EXPECT_EQ(rRect.radii()[1], (Point{10, 10}));
  EXPECT_EQ(rRect.radii()[2], (Point{5, 5}));
  EXPECT_EQ(rRect.radii()[3], (Point{30, 30}));
}

TGFX_TEST(RRectTest, RadiiScaling) {
  // Adjacent corner radii that exceed the edge length should be scaled down proportionally.
  // Top edge: TL.x + TR.x = 60 + 60 = 120 > 100, so scale factor = 100/120 ~= 0.833.
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 100), {{{60, 10}, {60, 10}, {10, 10}, {10, 10}}});
  // All radii should be scaled by the same factor.
  EXPECT_LE(rRect.radii()[0].x + rRect.radii()[1].x, 100.f + 1e-5f);
  // The original ratio between corners should be preserved.
  EXPECT_NEAR(rRect.radii()[0].x, rRect.radii()[1].x, 1e-5f);
  // ScaleRadii applies the single minimum scale factor (5/6) to every corner uniformly,
  // preserving the overall shape proportions.
  constexpr float scale = 100.f / 120.f;
  EXPECT_NEAR(rRect.radii()[0].x, 60.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[0].y, 10.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[1].x, 60.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[1].y, 10.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[2].x, 10.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[2].y, 10.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[3].x, 10.f * scale, 1e-5f);
  EXPECT_NEAR(rRect.radii()[3].y, 10.f * scale, 1e-5f);
}

TGFX_TEST(RRectTest, NegativeRadii) {
  // Negative radii should be clamped to zero.
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{-10, -5}, {10, 10}, {10, 10}, {10, 10}}});
  EXPECT_EQ(rRect.radii()[0], (Point{0, 0}));
  EXPECT_EQ(rRect.radii()[1], (Point{10, 10}));
}

TGFX_TEST(RRectTest, Scale) {
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {10, 10}, {5, 5}, {15, 15}}});
  rRect.scale(2.0f, 0.5f);
  EXPECT_FLOAT_EQ(rRect.rect().width(), 200.f);
  EXPECT_FLOAT_EQ(rRect.rect().height(), 40.f);
  EXPECT_FLOAT_EQ(rRect.radii()[0].x, 40.f);
  EXPECT_FLOAT_EQ(rRect.radii()[0].y, 5.f);
  EXPECT_FLOAT_EQ(rRect.radii()[2].x, 10.f);
  EXPECT_FLOAT_EQ(rRect.radii()[2].y, 2.5f);
}

TGFX_TEST(RRectTest, MakeTransformIdentity) {
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {10, 10}, {5, 5}, {15, 15}}});
  auto out = rRect.makeTransform(Matrix::I());
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->rect(), rRect.rect());
  EXPECT_EQ(out->radii(), rRect.radii());
  EXPECT_EQ(out->type(), rRect.type());
}

TGFX_TEST(RRectTest, MakeTransformTranslate) {
  auto rRect = RRect::MakeRectXY(Rect::MakeXYWH(10, 20, 100, 80), 8, 12);
  auto out = rRect.makeTransform(Matrix::MakeTrans(5, -3));
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->rect(), Rect::MakeXYWH(15, 17, 100, 80));
  EXPECT_EQ(out->radii()[0], (Point{8, 12}));
  EXPECT_EQ(out->type(), RRect::Type::Simple);
}

TGFX_TEST(RRectTest, MakeTransformScale) {
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {10, 10}, {5, 5}, {15, 15}}});
  auto out = rRect.makeTransform(Matrix::MakeScale(2, 0.5f));
  ASSERT_TRUE(out.has_value());
  EXPECT_FLOAT_EQ(out->rect().width(), 200.f);
  EXPECT_FLOAT_EQ(out->rect().height(), 40.f);
  // Top-left corner radii scale with the matrix, no swap on positive scale.
  EXPECT_FLOAT_EQ(out->radii()[0].x, 40.f);
  EXPECT_FLOAT_EQ(out->radii()[0].y, 5.f);
}

TGFX_TEST(RRectTest, MakeTransformMirrorX) {
  // flipX swaps left/right corner pairs.
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {30, 15}, {5, 5}, {15, 15}}});
  auto out = rRect.makeTransform(Matrix::MakeScale(-1, 1));
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->rect(), Rect::MakeXYWH(-100, 0, 100, 80));
  // After flipX: TL <-> TR, BR <-> BL.
  EXPECT_EQ(out->radii()[0], (Point{30, 15}));  // was TR
  EXPECT_EQ(out->radii()[1], (Point{20, 10}));  // was TL
  EXPECT_EQ(out->radii()[2], (Point{15, 15}));  // was BL
  EXPECT_EQ(out->radii()[3], (Point{5, 5}));    // was BR
}

TGFX_TEST(RRectTest, MakeTransformMirrorY) {
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {30, 15}, {5, 5}, {15, 15}}});
  auto out = rRect.makeTransform(Matrix::MakeScale(1, -1));
  ASSERT_TRUE(out.has_value());
  // After flipY: TL <-> BL, TR <-> BR.
  EXPECT_EQ(out->radii()[0], (Point{15, 15}));  // was BL
  EXPECT_EQ(out->radii()[1], (Point{5, 5}));    // was BR
  EXPECT_EQ(out->radii()[2], (Point{30, 15}));  // was TR
  EXPECT_EQ(out->radii()[3], (Point{20, 10}));  // was TL
}

TGFX_TEST(RRectTest, MakeTransformRotate90) {
  // 90 degrees clockwise: width <-> height, corner shift, x/y swap.
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {30, 15}, {5, 5}, {15, 15}}});
  Matrix m = {};
  m.setRotate(90);
  auto out = rRect.makeTransform(m);
  ASSERT_TRUE(out.has_value());
  EXPECT_FLOAT_EQ(out->rect().width(), 80.f);
  EXPECT_FLOAT_EQ(out->rect().height(), 100.f);
  // Top-left of new rect comes from bottom-left of old rect (clockwise rotation),
  // with x/y swapped.
  EXPECT_FLOAT_EQ(out->radii()[0].x, 15.f);  // old BL.y
  EXPECT_FLOAT_EQ(out->radii()[0].y, 15.f);  // old BL.x
}

TGFX_TEST(RRectTest, MakeTransformRotate270) {
  // 270 degrees clockwise (= 90 ccw).
  auto rRect =
      RRect::MakeRectRadii(Rect::MakeWH(100, 80), {{{20, 10}, {30, 15}, {5, 5}, {15, 15}}});
  Matrix m = {};
  m.setRotate(270);
  auto out = rRect.makeTransform(m);
  ASSERT_TRUE(out.has_value());
  EXPECT_FLOAT_EQ(out->rect().width(), 80.f);
  EXPECT_FLOAT_EQ(out->rect().height(), 100.f);
  // Top-left of new rect comes from top-right of old rect (counter-clockwise rotation),
  // with x/y swapped.
  EXPECT_FLOAT_EQ(out->radii()[0].x, 15.f);  // old TR.y
  EXPECT_FLOAT_EQ(out->radii()[0].y, 30.f);  // old TR.x
}

TGFX_TEST(RRectTest, MakeTransformComposite) {
  // scale * rotate90 should produce a valid axis-aligned RRect.
  auto rRect = RRect::MakeRectXY(Rect::MakeWH(100, 80), 8, 12);
  Matrix m = Matrix::MakeScale(2, 3);
  Matrix r = {};
  r.setRotate(90);
  m.preConcat(r);
  auto out = rRect.makeTransform(m);
  ASSERT_TRUE(out.has_value());
  EXPECT_FLOAT_EQ(out->rect().width(), 80.f * 2.f);
  EXPECT_FLOAT_EQ(out->rect().height(), 100.f * 3.f);
}

TGFX_TEST(RRectTest, MakeTransformShearFails) {
  auto rRect = RRect::MakeRectXY(Rect::MakeWH(100, 80), 8, 12);
  Matrix m = Matrix::I();
  m.setSkew(0.5f, 0);
  EXPECT_FALSE(rRect.makeTransform(m).has_value());
}

TGFX_TEST(RRectTest, MakeTransformPerspectiveFails) {
  auto rRect = RRect::MakeRectXY(Rect::MakeWH(100, 80), 8, 12);
  Matrix m = Matrix::I();
  m.setAll(1, 0, 0, 0, 1, 0, 0.001f, 0, 1);
  EXPECT_FALSE(rRect.makeTransform(m).has_value());
}

TGFX_TEST(RRectTest, MakeTransformExtremeScaleEmptyFails) {
  auto rRect = RRect::MakeRectXY(Rect::MakeWH(100, 80), 8, 12);
  // Zero scale collapses the rect.
  auto out = rRect.makeTransform(Matrix::MakeScale(0, 1));
  EXPECT_FALSE(out.has_value());
}

TGFX_TEST(RRectTest, MakeTransformOval) {
  auto rRect = RRect::MakeOval(Rect::MakeWH(100, 80));
  EXPECT_EQ(rRect.type(), RRect::Type::Oval);
  auto out = rRect.makeTransform(Matrix::MakeScale(2, 1));
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->type(), RRect::Type::Oval);
  EXPECT_FLOAT_EQ(out->radii()[0].x, 100.f);
  EXPECT_FLOAT_EQ(out->radii()[0].y, 40.f);
}

TGFX_TEST(RRectTest, MakeTransformRect) {
  auto rRect = RRect::MakeRectXY(Rect::MakeWH(100, 80), 0, 0);
  EXPECT_EQ(rRect.type(), RRect::Type::Rect);
  auto out = rRect.makeTransform(Matrix::MakeScale(2, 0.5f));
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->type(), RRect::Type::Rect);
  EXPECT_FLOAT_EQ(out->rect().width(), 200.f);
  EXPECT_FLOAT_EQ(out->rect().height(), 40.f);
}

}  // namespace tgfx
