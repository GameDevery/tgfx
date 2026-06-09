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

#include "layers/contents/LayerGeometryUtils.h"
#include <algorithm>
#include "tgfx/core/RRect.h"
#include "tgfx/core/Shape.h"

namespace tgfx {

std::optional<StyledShape> MakeBoundsStyledShape(const Path& path) {
  StyledShape geometry = {};
  if (!path.isEmpty()) {
    Path boundsPath = {};
    boundsPath.addRect(path.getBounds());
    geometry.shape = Shape::MakeFrom(boundsPath);
  }
  geometry.style = PaintStyle::Fill;
  return geometry;
}

std::optional<StyledShape> MakeOutsetMergedStyledShape(const Path& path, float strokeWidth) {
  auto halfStroke = strokeWidth * 0.5f;
  if (!CanSpreadAsRRect(path)) {
    return MakeBoundsStyledShape(path);
  }
  auto outset = MakeOutsetShape(path, halfStroke);
  if (outset.isEmpty()) {
    return MakeBoundsStyledShape(path);
  }
  StyledShape geometry = {};
  geometry.shape = Shape::MakeFrom(outset);
  geometry.style = PaintStyle::Fill;
  return geometry;
}

bool CanSpreadAsRRect(const Path& path) {
  if (path.isEmpty()) {
    return false;
  }
  return path.isRect() || path.isOval() || path.isRRect(nullptr);
}

Path GetSpreadableFillPath(const Path& path) {
  if (path.isEmpty()) {
    return {};
  }
  if (CanSpreadAsRRect(path)) {
    return path;
  }
  Path boundsPath = {};
  boundsPath.addRect(path.getBounds());
  return boundsPath;
}

Path MakeOutsetShape(const Path& path, float distance) {
  Rect rect = {};
  if (path.isOval(&rect)) {
    rect.outset(distance, distance);
    Path result = {};
    result.addOval(rect);
    return result;
  }
  if (path.isRect(&rect)) {
    rect.outset(distance, distance);
    Path result = {};
    result.addRect(rect);
    return result;
  }
  RRect rRect = {};
  if (path.isRRect(&rRect)) {
    Path result = {};
    result.addRRect(MakeOutsetRRect(rRect, distance));
    return result;
  }
  return {};
}

Path MakeInsetShape(const Path& path, float distance) {
  Rect rect = {};
  if (path.isOval(&rect)) {
    rect.outset(-distance, -distance);
    if (rect.width() <= 0.0f || rect.height() <= 0.0f) {
      return {};
    }
    Path result = {};
    result.addOval(rect);
    return result;
  }
  if (path.isRect(&rect)) {
    rect.outset(-distance, -distance);
    if (rect.width() <= 0.0f || rect.height() <= 0.0f) {
      return {};
    }
    Path result = {};
    result.addRect(rect);
    return result;
  }
  RRect rRect = {};
  if (path.isRRect(&rRect)) {
    auto insetRRect = MakeInsetRRect(rRect, distance);
    if (!insetRRect.has_value()) {
      return {};
    }
    Path result = {};
    result.addRRect(*insetRRect);
    return result;
  }
  return {};
}

RRect MakeOutsetRRect(const RRect& rRect, float distance) {
  auto bounds = rRect.rect();
  bounds.outset(distance, distance);
  auto radii = rRect.radii();
  for (auto& corner : radii) {
    corner.x += distance;
    corner.y += distance;
  }
  RRect result = {};
  result.setRectRadii(bounds, radii);
  return result;
}

std::optional<RRect> MakeInsetRRect(const RRect& rRect, float distance) {
  auto bounds = rRect.rect();
  bounds.outset(-distance, -distance);
  if (bounds.width() <= 0.0f || bounds.height() <= 0.0f) {
    return std::nullopt;
  }
  auto radii = rRect.radii();
  for (auto& corner : radii) {
    corner.x = std::max(0.0f, corner.x - distance);
    corner.y = std::max(0.0f, corner.y - distance);
  }
  RRect result = {};
  result.setRectRadii(bounds, radii);
  return result;
}

}  // namespace tgfx
