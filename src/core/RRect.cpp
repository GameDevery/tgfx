/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "tgfx/core/RRect.h"
#include <cmath>

namespace tgfx {

// Zeros out the smaller of two values when it is fully absorbed by the larger one in float
// addition (a + b == a).
static inline void FlushToZero(float& a, float& b) {
  if (a + b == a) {
    b = 0;
  }
  if (a + b == b) {
    a = 0;
  }
}

// If a + b exceeds limit, lowers scale to limit / (a + b) (unless the current scale is already
// smaller). Uses double so a small value is not absorbed by a much larger one, which would hide
// the fact that their sum exceeds the limit.
static inline void UpdateMinScale(float a, float b, double limit, double& scale) {
  const auto sum = static_cast<double>(a) + static_cast<double>(b);
  if (sum > 0 && limit / sum < scale) {
    scale = limit / sum;
  }
}

// Scales per-corner radii so that no two adjacent radii overflow the side they share.
static inline void ScaleRadii(const Rect& rect, std::array<Point, 4>& radii) {
  const auto width = static_cast<double>(rect.width());
  const auto height = static_cast<double>(rect.height());

  double scale = 1.0;
  UpdateMinScale(radii[0].x, radii[1].x, width, scale);
  UpdateMinScale(radii[1].y, radii[2].y, height, scale);
  UpdateMinScale(radii[2].x, radii[3].x, width, scale);
  UpdateMinScale(radii[3].y, radii[0].y, height, scale);

  if (scale < 1.0) {
    // Note: double-to-float conversion plus float addition may leave the sum of two adjacent
    // radii slightly greater than the side length (1-2 ULPs). Downstream rendering does not rely
    // on the strict a + b <= limit invariant, so we do not correct it here.
    for (auto& r : radii) {
      r.x = static_cast<float>(static_cast<double>(r.x) * scale);
      r.y = static_cast<float>(static_cast<double>(r.y) * scale);
    }
  }

  // After scaling, the smaller of two adjacent radii may be fully absorbed by the larger one
  // (a + b == a in float). Downstream geometry can still read b on its own as a non-zero value,
  // which disagrees with the sum result and produces seams at the boundary.
  FlushToZero(radii[0].x, radii[1].x);
  FlushToZero(radii[1].y, radii[2].y);
  FlushToZero(radii[2].x, radii[3].x);
  FlushToZero(radii[3].y, radii[0].y);

  // If either component of a corner is zero, the corner is effectively square: the other
  // component can no longer describe a curve.
  for (auto& r : radii) {
    if (r.x <= 0 || r.y <= 0) {
      r = {0, 0};
    }
  }
}

static inline RRect::Type ComputeType(const Rect& rect, const std::array<Point, 4>& radii) {
  if (rect.isEmpty()) {
    return RRect::Type::Rect;
  }
  auto allZero = true;
  for (const auto& r : radii) {
    if (r.x != 0 || r.y != 0) {
      allZero = false;
      break;
    }
  }
  if (allZero) {
    return RRect::Type::Rect;
  }
  if (radii[0] == radii[1] && radii[1] == radii[2] && radii[2] == radii[3]) {
    const auto halfWidth = rect.width() * 0.5f;
    const auto halfHeight = rect.height() * 0.5f;
    // Use strict comparison instead of an epsilon tolerance: RRect lives in logical space that
    // may later be rescaled by an arbitrary transform, so any tolerance chosen here has no
    // stable pixel-level meaning after the transform.
    if (radii[0].x >= halfWidth && radii[0].y >= halfHeight) {
      return RRect::Type::Oval;
    }
    return RRect::Type::Simple;
  }
  return RRect::Type::Complex;
}

void RRect::setRect(const Rect& rect) {
  _rect = rect.makeSorted();
  _radii = {};
  _type = Type::Rect;
}

void RRect::setRectXY(const Rect& rect, float radiusX, float radiusY) {
  const auto radius = Point{radiusX, radiusY};
  setRectRadii(rect, {radius, radius, radius, radius});
}

void RRect::setRectRadii(const Rect& rect, const std::array<Point, 4>& radii) {
  _rect = rect.makeSorted();
  _radii = radii;
  for (auto& rad : _radii) {
    if (rad.x < 0 || rad.y < 0) {
      rad = {0, 0};
    }
  }
  ScaleRadii(_rect, _radii);
  _type = ComputeType(_rect, _radii);
}

void RRect::setOval(const Rect& oval) {
  const auto sorted = oval.makeSorted();
  const auto radius = Point{sorted.width() / 2, sorted.height() / 2};
  setRectRadii(sorted, {radius, radius, radius, radius});
}

// Positive scaling preserves all RRect classification invariants (adjacent-radii fit,
// Oval/Simple/Complex/Rect thresholds), so ScaleRadii and ComputeType are intentionally
// skipped here. Non-positive factors are out of contract.
void RRect::scale(float scaleX, float scaleY) {
  _rect.scale(scaleX, scaleY);
  for (auto& r : _radii) {
    r.x *= scaleX;
    r.y *= scaleY;
  }
}

void RRect::offset(float dx, float dy) {
  _rect.offset(dx, dy);
}

// Corner index convention used by SkRRect (matches tgfx layout):
//   0 = top-left, 1 = top-right, 2 = bottom-right, 3 = bottom-left
std::optional<RRect> RRect::makeTransform(const Matrix& matrix) const {
  if (matrix.isIdentity()) {
    return *this;
  }
  if (!matrix.rectStaysRect()) {
    return std::nullopt;
  }

  Rect newRect = matrix.mapRect(_rect);
  // mapRect produces a sorted rect under axis-aligned matrices, so an empty rect indicates
  // a collapsed dimension (loss of precision). Non-finite results indicate overflow.
  if (!std::isfinite(newRect.left) || !std::isfinite(newRect.top) ||
      !std::isfinite(newRect.right) || !std::isfinite(newRect.bottom) || newRect.isEmpty()) {
    return std::nullopt;
  }

  RRect dst = {};
  dst._rect = newRect;
  dst._type = _type;

  if (_type == Type::Rect) {
    return dst;
  }
  if (_type == Type::Oval) {
    const auto rx = newRect.width() * 0.5f;
    const auto ry = newRect.height() * 0.5f;
    for (auto& r : dst._radii) {
      r = {rx, ry};
    }
    return dst;
  }

  float xScale = matrix.getScaleX();
  float yScale = matrix.getScaleY();

  // 90 / 270 degree rotation: scale entries are zero and skew entries carry the rotation.
  // 180 degrees rotations are simply flipX with a flipY and would come under a scale transform.
  if (!matrix.isScaleTranslate()) {
    const bool isClockwise = matrix.getSkewX() < 0;
    yScale = matrix.getSkewY() * (isClockwise ? 1.0f : -1.0f);
    xScale = matrix.getSkewX() * (isClockwise ? -1.0f : 1.0f);

    const int dir = isClockwise ? 3 : 1;
    for (size_t i = 0; i < 4; ++i) {
      const auto src = static_cast<size_t>(static_cast<int>(i) + dir) % 4;
      // Swap X and Y axis for the radii.
      dst._radii[i].x = _radii[src].y;
      dst._radii[i].y = _radii[src].x;
    }
  } else {
    dst._radii = _radii;
  }

  const bool flipX = xScale < 0;
  if (flipX) {
    xScale = -xScale;
  }
  const bool flipY = yScale < 0;
  if (flipY) {
    yScale = -yScale;
  }

  for (auto& r : dst._radii) {
    r.x *= xScale;
    r.y *= yScale;
  }

  if (flipX) {
    if (flipY) {
      // Swap with opposite corners.
      std::swap(dst._radii[0], dst._radii[2]);
      std::swap(dst._radii[1], dst._radii[3]);
    } else {
      // Only swap in x.
      std::swap(dst._radii[0], dst._radii[1]);
      std::swap(dst._radii[2], dst._radii[3]);
    }
  } else if (flipY) {
    // Only swap in y.
    std::swap(dst._radii[0], dst._radii[3]);
    std::swap(dst._radii[1], dst._radii[2]);
  }

  ScaleRadii(dst._rect, dst._radii);
  dst._type = ComputeType(dst._rect, dst._radii);
  return dst;
}
}  // namespace tgfx
