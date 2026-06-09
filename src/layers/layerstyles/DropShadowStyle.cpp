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

#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "core/utils/StrokeUtils.h"
#include "layers/contents/LayerGeometryUtils.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PictureRecorder.h"
#include "tgfx/core/Shape.h"
#include "tgfx/core/Stroke.h"

namespace tgfx {

struct SpreadGeometry {
  Path path = {};
  std::optional<Stroke> stroke = std::nullopt;
};

static SpreadGeometry BuildSpreadGeometry(const StyledShape& geometry, float spread) {
  auto path = geometry.shape ? geometry.shape->getPath() : Path{};
  if (path.isEmpty()) {
    return {};
  }
  if (geometry.style == PaintStyle::Fill) {
    auto spreadPath = GetSpreadableFillPath(path);
    if (spreadPath.isEmpty()) {
      return {};
    }
    if (spread > 0) {
      return {MakeOutsetShape(spreadPath, spread), std::nullopt};
    }
    if (spread < 0) {
      auto result = MakeInsetShape(spreadPath, -spread);
      return {result.isEmpty() ? Path{} : result, std::nullopt};
    }
    return {spreadPath, std::nullopt};
  }
  auto halfStroke = geometry.strokeWidth * 0.5f;
  auto outerHalfWidth = halfStroke + spread;
  if (outerHalfWidth <= 0.0f) {
    return {};
  }
  Stroke widened(2.0f * outerHalfWidth);
  return {path, widened};
}

struct ShadowSourceImage {
  std::shared_ptr<Image> image = nullptr;
  Point offset = {};
};

// Produces the alpha source image fed into the shadow ImageFilter. When contentShape is available
// and the style has a non-zero spread, this rasterizes the spread-applied geometry into a
// tightly-sized image; otherwise it returns source.content unchanged. The returned offset is the
// top-left of the produced image in the coordinate space the caller draws into.
static ShadowSourceImage MakeShadowSourceImage(const LayerStyleDrawSource& source, float spread) {
  if (!source.contentShape.has_value() || spread == 0) {
    return {source.content, {}};
  }
  auto spreadGeometry = BuildSpreadGeometry(*source.contentShape, spread);
  if (spreadGeometry.path.isEmpty()) {
    return {nullptr, {}};
  }
  auto pathBound = spreadGeometry.path.getBounds();
  if (spreadGeometry.stroke.has_value()) {
    ApplyStrokeToBounds(*spreadGeometry.stroke, &pathBound);
  }
  auto pixelBound = pathBound;
  pixelBound.scale(source.contentScale, source.contentScale);
  pixelBound.roundOut();
  auto width = static_cast<int>(pixelBound.width());
  auto height = static_cast<int>(pixelBound.height());
  if (width <= 0 || height <= 0) {
    return {nullptr, {}};
  }
  PictureRecorder recorder;
  auto* recordCanvas = recorder.beginRecording();
  recordCanvas->translate(-pixelBound.left, -pixelBound.top);
  recordCanvas->scale(source.contentScale, source.contentScale);
  Paint paint = {};
  paint.setColor(Color::Black());
  paint.setAntiAlias(true);
  if (spreadGeometry.stroke.has_value()) {
    paint.setStyle(PaintStyle::Stroke);
    paint.setStroke(*spreadGeometry.stroke);
  }
  recordCanvas->drawPath(spreadGeometry.path, paint);
  auto picture = recorder.finishRecordingAsPicture();
  if (picture == nullptr) {
    return {nullptr, {}};
  }
  auto image = Image::MakeFrom(std::move(picture), width, height);
  return {std::move(image), {pixelBound.left, pixelBound.top}};
}

std::shared_ptr<DropShadowStyle> DropShadowStyle::Make(float offsetX, float offsetY,
                                                       float blurrinessX, float blurrinessY,
                                                       const Color& color, bool showBehindLayer) {
  return std::shared_ptr<DropShadowStyle>(
      new DropShadowStyle(offsetX, offsetY, blurrinessX, blurrinessY, color, showBehindLayer));
}

void DropShadowStyle::setOffsetX(float offsetX) {
  if (_offsetX == offsetX) {
    return;
  }
  _offsetX = offsetX;
  invalidateFilter();
}

void DropShadowStyle::setOffsetY(float offsetY) {
  if (_offsetY == offsetY) {
    return;
  }
  _offsetY = offsetY;
  invalidateFilter();
}

void DropShadowStyle::setBlurrinessX(float blurrinessX) {
  if (_blurrinessX == blurrinessX) {
    return;
  }
  _blurrinessX = blurrinessX;
  invalidateFilter();
}

void DropShadowStyle::setBlurrinessY(float blurrinessY) {
  if (_blurrinessY == blurrinessY) {
    return;
  }
  _blurrinessY = blurrinessY;
  invalidateFilter();
}

void DropShadowStyle::setColor(const Color& color) {
  if (_color == color) {
    return;
  }
  _color = color;
  invalidateFilter();
}

void DropShadowStyle::setShowBehindLayer(bool showBehindLayer) {
  if (_showBehindLayer == showBehindLayer) {
    return;
  }
  _showBehindLayer = showBehindLayer;
  invalidateTransform();
}

void DropShadowStyle::setSpread(float spread) {
  if (_spread == spread) {
    return;
  }
  _spread = spread;
  // Spread only affects the geometric path; the cached ImageFilter is unaffected. Trigger a
  // redraw so the geometric branch picks up the new value.
  invalidateTransform();
}

DropShadowStyle::DropShadowStyle(float offsetX, float offsetY, float blurrinessX, float blurrinessY,
                                 const Color& color, bool showBehindLayer)
    : _offsetX(offsetX), _offsetY(offsetY), _blurrinessX(blurrinessX), _blurrinessY(blurrinessY),
      _color(color), _showBehindLayer(showBehindLayer) {
}

Rect DropShadowStyle::filterBounds(const Rect& srcRect, float contentScale) {
  // When spread is non-zero, conservatively size bounds for the geometric path. The actual draw
  // call decides whether to take that path based on whether the Layer provides a path source.
  if (_spread != 0) {
    auto bounds = srcRect;
    auto spreadOutset = std::max(_spread, 0.0f) * contentScale;
    auto blurOutsetX = _blurrinessX * 2.0f * contentScale;
    auto blurOutsetY = _blurrinessY * 2.0f * contentScale;
    bounds.outset(spreadOutset + blurOutsetX, spreadOutset + blurOutsetY);
    bounds.offset(_offsetX * contentScale, _offsetY * contentScale);
    bounds.join(srcRect);
    return bounds;
  }
  auto filter = getShadowFilter(contentScale);
  if (!filter) {
    return srcRect;
  }
  return filter->filterBounds(srcRect);
}

void DropShadowStyle::onDraw(Canvas* canvas, const LayerStyleDrawSource& source, float alpha,
                             BlendMode blendMode) {
  auto shadowSource = MakeShadowSourceImage(source, _spread);
  if (shadowSource.image == nullptr) {
    return;
  }
  auto filter = getShadowFilter(source.contentScale);
  if (!filter) {
    return;
  }
  Point filterOffset = {};
  auto shadowImage = shadowSource.image->makeWithFilter(filter, &filterOffset);
  // Use nearest filtering when there's no blur to avoid edge artifacts caused by linear
  // interpolation. When the texture is scaled up, linear filtering produces intermediate alpha
  // values at edges, which causes visible borders in the shadow.
  auto sampling = (_blurrinessX == 0 && _blurrinessY == 0)
                      ? SamplingOptions(FilterMode::Nearest, MipmapMode::None)
                      : SamplingOptions();
  Paint paint = {};
  if (!_showBehindLayer && source.extra != nullptr) {
    auto shader = Shader::MakeImageShader(source.extra, TileMode::Decal, TileMode::Decal, sampling);
    auto matrixShader =
        shader->makeWithMatrix(Matrix::MakeTrans(source.extraOffset.x, source.extraOffset.y));
    paint.setMaskFilter(MaskFilter::MakeShader(matrixShader, true));
  }
  paint.setBlendMode(blendMode);
  paint.setAlpha(alpha);
  canvas->drawImage(shadowImage, shadowSource.offset.x + filterOffset.x,
                    shadowSource.offset.y + filterOffset.y, sampling, &paint);
}

std::shared_ptr<ImageFilter> DropShadowStyle::getShadowFilter(float scale) {
  if (shadowFilter && scale == currentScale) {
    return shadowFilter;
  }

  shadowFilter = ImageFilter::DropShadowOnly(_offsetX * scale, _offsetY * scale,
                                             _blurrinessX * scale, _blurrinessY * scale, _color);
  currentScale = scale;

  return shadowFilter;
}

void DropShadowStyle::invalidateFilter() {
  shadowFilter = nullptr;
  invalidateTransform();
}

}  // namespace tgfx
