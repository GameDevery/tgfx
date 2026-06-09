/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "tgfx/layers/layerstyles/InnerShadowStyle.h"
#include "layers/contents/LayerGeometryUtils.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Shape.h"
#include "tgfx/core/Stroke.h"

namespace tgfx {

// Returns the path that represents the inner-shadow casting region: the original layer footprint
// minus the inset / outset / offset shadow body. The result is intentionally clipped against the
// original geometry by the caller via canvas->clipPath() so the shadow never escapes the layer.
// Returns an empty path either when the shadow body still covers the full footprint (nothing to
// draw) or when the supplied geometry cannot be analytically resolved as RRect / Oval / Rect.
static Path BuildInnerShadowPath(const StyledShape& geometry, float spread, float offsetX,
                                 float offsetY) {
  auto rawPath = geometry.shape ? geometry.shape->getPath() : Path{};
  auto path = GetSpreadableFillPath(rawPath);
  if (path.isEmpty()) {
    return {};
  }
  Path outerFill = path;
  Path innerHole = {};
  bool hasInnerHole = false;
  if (geometry.style == PaintStyle::Stroke) {
    auto halfStroke = geometry.strokeWidth * 0.5f;
    outerFill = MakeOutsetShape(path, halfStroke);
    auto candidate = MakeInsetShape(path, halfStroke);
    if (!candidate.isEmpty()) {
      innerHole = candidate;
      hasInnerHole = true;
    }
  }
  Path shadowBody = path;
  if (spread > 0) {
    shadowBody = MakeInsetShape(path, spread);
  } else if (spread < 0) {
    shadowBody = MakeOutsetShape(path, -spread);
  }
  // Apply the light-direction offset: shadow body translates by (offsetX, offsetY) in path
  // space; everything outside the translated body, but inside the original layer, is shadow.
  if (offsetX != 0 || offsetY != 0) {
    shadowBody.transform(Matrix::MakeTrans(offsetX, offsetY));
  }
  if (shadowBody.isEmpty()) {
    // The body collapsed; the entire layer becomes shadow.
    return outerFill;
  }
  auto shadowFill =
      Shape::Merge(Shape::MakeFrom(outerFill), Shape::MakeFrom(shadowBody), PathOp::Difference);
  if (hasInnerHole) {
    // Stroke source: also subtract the original hole, so inner shadow only fills the ring.
    shadowFill = Shape::Merge(shadowFill, Shape::MakeFrom(innerHole), PathOp::Difference);
  }
  return shadowFill ? shadowFill->getPath() : Path{};
}

std::shared_ptr<InnerShadowStyle> InnerShadowStyle::Make(float offsetX, float offsetY,
                                                         float blurrinessX, float blurrinessY,
                                                         const Color& color) {
  return std::shared_ptr<InnerShadowStyle>(
      new InnerShadowStyle(offsetX, offsetY, blurrinessX, blurrinessY, color));
}

void InnerShadowStyle::setOffsetX(float offsetX) {
  if (_offsetX == offsetX) {
    return;
  }
  _offsetX = offsetX;
  invalidateFilter();
}

void InnerShadowStyle::setOffsetY(float offsetY) {
  if (_offsetY == offsetY) {
    return;
  }
  _offsetY = offsetY;
  invalidateFilter();
}

void InnerShadowStyle::setBlurrinessX(float blurrinessX) {
  if (_blurrinessX == blurrinessX) {
    return;
  }
  _blurrinessX = blurrinessX;
  invalidateFilter();
}

void InnerShadowStyle::setBlurrinessY(float blurrinessY) {
  if (_blurrinessY == blurrinessY) {
    return;
  }
  _blurrinessY = blurrinessY;
  invalidateFilter();
}

void InnerShadowStyle::setColor(const Color& color) {
  if (_color == color) {
    return;
  }
  _color = color;
  invalidateFilter();
}

void InnerShadowStyle::setSpread(float spread) {
  if (_spread == spread) {
    return;
  }
  _spread = spread;
  invalidateTransform();
}

InnerShadowStyle::InnerShadowStyle(float offsetX, float offsetY, float blurrinessX,
                                   float blurrinessY, const Color& color)
    : _offsetX(offsetX), _offsetY(offsetY), _blurrinessX(blurrinessX), _blurrinessY(blurrinessY),
      _color(color) {
}

Rect InnerShadowStyle::filterBounds(const Rect& srcRect, float contentScale) {
  if (_spread != 0) {
    // Inner shadow stays inside the layer footprint; bounds equal srcRect even when spread is
    // non-zero. The actual draw call decides whether to take the geometric path.
    return srcRect;
  }
  auto filter = getShadowFilter(contentScale);
  if (!filter) {
    return srcRect;
  }
  return filter->filterBounds(srcRect);
}

void InnerShadowStyle::onDraw(Canvas* canvas, const LayerStyleDrawSource& source, float alpha,
                              BlendMode blendMode) {
  if (source.contentShape.has_value() && _spread != 0) {
    auto geoPath = source.contentShape->shape ? source.contentShape->shape->getPath() : Path{};
    if (CanSpreadAsRRect(geoPath)) {
      auto shadowPath = BuildInnerShadowPath(*source.contentShape, _spread, _offsetX, _offsetY);
      if (shadowPath.isEmpty()) {
        return;
      }
      AutoCanvasRestore guard(canvas);
      canvas->clipPath(geoPath);
      Paint paint = {};
      paint.setColor(_color);
      paint.setAlpha(_color.alpha * alpha);
      paint.setBlendMode(blendMode);
      paint.setAntiAlias(true);
      if (_blurrinessX > 0 || _blurrinessY > 0) {
        paint.setImageFilter(ImageFilter::Blur(_blurrinessX, _blurrinessY));
      }
      canvas->drawPath(shadowPath, paint);
      return;
    }
  }
  auto filter = getShadowFilter(source.contentScale);
  if (!filter) {
    return;
  }
  auto content = source.content->makeWithFilter(filter);
  Paint paint = {};
  paint.setBlendMode(blendMode);
  paint.setAlpha(alpha);
  // Use nearest filtering when there's no blur to avoid edge artifacts caused by linear
  // interpolation. When the texture is scaled up, linear filtering produces intermediate alpha
  // values at edges, which causes visible gray borders in the inner shadow.
  auto sampling = SamplingOptions();
  if (_blurrinessX == 0 && _blurrinessY == 0) {
    sampling = SamplingOptions(FilterMode::Nearest, MipmapMode::None);
  }
  canvas->drawImage(content, sampling, &paint);
}

std::shared_ptr<ImageFilter> InnerShadowStyle::getShadowFilter(float scale) {
  if (shadowFilter && scale == currentScale) {
    return shadowFilter;
  }

  shadowFilter = ImageFilter::InnerShadowOnly(_offsetX * scale, _offsetY * scale,
                                              _blurrinessX * scale, _blurrinessY * scale, _color);
  currentScale = scale;

  return shadowFilter;
}

void InnerShadowStyle::invalidateFilter() {
  shadowFilter = nullptr;
  currentScale = 0.0f;
  invalidateTransform();
}

}  // namespace tgfx
