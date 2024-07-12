// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/geometry/path.h"

#include <optional>
#include <variant>

#include "impeller/geometry/path_component.h"

namespace impeller {

Path::Path() {
  AddContourComponent({});
};

Path::~Path() = default;

std::tuple<size_t, size_t> Path::Polyline::GetContourPointBounds(
    size_t contour_index) const {
  if (contour_index >= contours.size()) {
    return {points.size(), points.size()};
  }
  const size_t start_index = contours.at(contour_index).start_index;
  const size_t end_index = (contour_index >= contours.size() - 1)
                               ? points.size()
                               : contours.at(contour_index + 1).start_index;
  return std::make_tuple(start_index, end_index);
}

size_t Path::GetComponentCount(std::optional<ComponentType> type) const {
  if (type.has_value()) {
    switch (type.value()) {
      case ComponentType::kLinear:
        return linears_.size();
      case ComponentType::kQuadratic:
        return quads_.size();
      case ComponentType::kCubic:
        return cubics_.size();
      case ComponentType::kContour:
        return contours_.size();
    }
  }
  return components_.size();
}

void Path::SetFillType(FillType fill) {
  fill_ = fill;
}

FillType Path::GetFillType() const {
  return fill_;
}

Path& Path::AddLinearComponent(Point p1, Point p2) {
  linears_.emplace_back(p1, p2);
  components_.emplace_back(ComponentType::kLinear, linears_.size() - 1);
  return *this;
}

Path& Path::AddQuadraticComponent(Point p1, Point cp, Point p2) {
  quads_.emplace_back(p1, cp, p2);
  components_.emplace_back(ComponentType::kQuadratic, quads_.size() - 1);
  return *this;
}

Path& Path::AddCubicComponent(Point p1, Point cp1, Point cp2, Point p2) {
  cubics_.emplace_back(p1, cp1, cp2, p2);
  components_.emplace_back(ComponentType::kCubic, cubics_.size() - 1);
  return *this;
}

Path& Path::AddContourComponent(Point destination, bool is_closed) {
  if (components_.size() > 0 &&
      components_.back().type == ComponentType::kContour) {
    // Never insert contiguous contours.
    contours_.back() = ContourComponent(destination, is_closed);
  } else {
    contours_.emplace_back(ContourComponent(destination, is_closed));
    components_.emplace_back(ComponentType::kContour, contours_.size() - 1);
  }
  return *this;
}

void Path::SetContourClosed(bool is_closed) {
  contours_.back().is_closed = is_closed;
}

void Path::EnumerateComponents(
    const Applier<LinearPathComponent>& linear_applier,
    const Applier<QuadraticPathComponent>& quad_applier,
    const Applier<CubicPathComponent>& cubic_applier,
    const Applier<ContourComponent>& contour_applier) const {
  size_t currentIndex = 0;
  for (const auto& component : components_) {
    switch (component.type) {
      case ComponentType::kLinear:
        if (linear_applier) {
          linear_applier(currentIndex, linears_[component.index]);
        }
        break;
      case ComponentType::kQuadratic:
        if (quad_applier) {
          quad_applier(currentIndex, quads_[component.index]);
        }
        break;
      case ComponentType::kCubic:
        if (cubic_applier) {
          cubic_applier(currentIndex, cubics_[component.index]);
        }
        break;
      case ComponentType::kContour:
        if (contour_applier) {
          contour_applier(currentIndex, contours_[component.index]);
        }
        break;
    }
    currentIndex++;
  }
}

bool Path::GetLinearComponentAtIndex(size_t index,
                                     LinearPathComponent& linear) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kLinear) {
    return false;
  }

  linear = linears_[components_[index].index];
  return true;
}

bool Path::GetQuadraticComponentAtIndex(
    size_t index,
    QuadraticPathComponent& quadratic) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kQuadratic) {
    return false;
  }

  quadratic = quads_[components_[index].index];
  return true;
}

bool Path::GetCubicComponentAtIndex(size_t index,
                                    CubicPathComponent& cubic) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kCubic) {
    return false;
  }

  cubic = cubics_[components_[index].index];
  return true;
}

bool Path::GetContourComponentAtIndex(size_t index,
                                      ContourComponent& move) const {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kContour) {
    return false;
  }

  move = contours_[components_[index].index];
  return true;
}

bool Path::UpdateLinearComponentAtIndex(size_t index,
                                        const LinearPathComponent& linear) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kLinear) {
    return false;
  }

  linears_[components_[index].index] = linear;
  return true;
}

bool Path::UpdateQuadraticComponentAtIndex(
    size_t index,
    const QuadraticPathComponent& quadratic) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kQuadratic) {
    return false;
  }

  quads_[components_[index].index] = quadratic;
  return true;
}

bool Path::UpdateCubicComponentAtIndex(size_t index,
                                       CubicPathComponent& cubic) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kCubic) {
    return false;
  }

  cubics_[components_[index].index] = cubic;
  return true;
}

bool Path::UpdateContourComponentAtIndex(size_t index,
                                         const ContourComponent& move) {
  if (index >= components_.size()) {
    return false;
  }

  if (components_[index].type != ComponentType::kContour) {
    return false;
  }

  contours_[components_[index].index] = move;
  return true;
}

Path::Polyline Path::CreatePolyline(Scalar scale) const {
  Polyline polyline;

  std::optional<Point> previous_contour_point;
  auto collect_points = [&polyline, &previous_contour_point](
                            const std::vector<Point>& collection) {
    if (collection.empty()) {
      return;
    }

    for (const auto& point : collection) {
      if (previous_contour_point.has_value() &&
          previous_contour_point.value() == point) {
        // Skip over duplicate points in the same contour.
        continue;
      }
      previous_contour_point = point;
      polyline.points.push_back(point);
    }
  };

  auto get_path_component = [this](size_t component_i) -> PathComponentVariant {
    if (component_i >= components_.size()) {
      return std::monostate{};
    }
    const auto& component = components_[component_i];
    switch (component.type) {
      case ComponentType::kLinear:
        return &linears_[component.index];
      case ComponentType::kQuadratic:
        return &quads_[component.index];
      case ComponentType::kCubic:
        return &cubics_[component.index];
      case ComponentType::kContour:
        return std::monostate{};
    }
  };

  auto compute_contour_start_direction =
      [&get_path_component](size_t current_path_component_index) {
        size_t next_component_index = current_path_component_index + 1;
        while (!std::holds_alternative<std::monostate>(
            get_path_component(next_component_index))) {
          auto next_component = get_path_component(next_component_index);
          auto maybe_vector =
              std::visit(PathComponentStartDirectionVisitor(), next_component);
          if (maybe_vector.has_value()) {
            return maybe_vector.value();
          } else {
            next_component_index++;
          }
        }
        return Vector2(0, -1);
      };

  std::optional<size_t> previous_path_component_index;
  auto end_contour = [&polyline, &previous_path_component_index,
                      &get_path_component]() {
    // Whenever a contour has ended, extract the exact end direction from the
    // last component.
    if (polyline.contours.empty()) {
      return;
    }

    if (!previous_path_component_index.has_value()) {
      return;
    }

    auto& contour = polyline.contours.back();
    contour.end_direction = Vector2(0, 1);

    size_t previous_index = previous_path_component_index.value();
    while (!std::holds_alternative<std::monostate>(
        get_path_component(previous_index))) {
      auto previous_component = get_path_component(previous_index);
      auto maybe_vector =
          std::visit(PathComponentEndDirectionVisitor(), previous_component);
      if (maybe_vector.has_value()) {
        contour.end_direction = maybe_vector.value();
        break;
      } else {
        if (previous_index == 0) {
          break;
        }
        previous_index--;
      }
    }
  };

  for (size_t component_i = 0; component_i < components_.size();
       component_i++) {
    const auto& component = components_[component_i];
    switch (component.type) {
      case ComponentType::kLinear:
        collect_points(linears_[component.index].CreatePolyline());
        previous_path_component_index = component_i;
        break;
      case ComponentType::kQuadratic:
        collect_points(quads_[component.index].CreatePolyline(scale));
        previous_path_component_index = component_i;
        break;
      case ComponentType::kCubic:
        collect_points(cubics_[component.index].CreatePolyline(scale));
        previous_path_component_index = component_i;
        break;
      case ComponentType::kContour:
        if (component_i == components_.size() - 1) {
          // If the last component is a contour, that means it's an empty
          // contour, so skip it.
          continue;
        }
        end_contour();

        Vector2 start_direction = compute_contour_start_direction(component_i);
        const auto& contour = contours_[component.index];
        polyline.contours.push_back({.start_index = polyline.points.size(),
                                     .is_closed = contour.is_closed,
                                     .start_direction = start_direction});
        previous_contour_point = std::nullopt;
        collect_points({contour.destination});
        break;
    }
    end_contour();
  }
  return polyline;
}

std::optional<Rect> Path::GetBoundingBox() const {
  auto min_max = GetMinMaxCoveragePoints();
  if (!min_max.has_value()) {
    return std::nullopt;
  }
  auto min = min_max->first;
  auto max = min_max->second;
  const auto difference = max - min;
  return Rect{min.x, min.y, difference.x, difference.y};
}

std::optional<Rect> Path::GetTransformedBoundingBox(
    const Matrix& transform) const {
  auto bounds = GetBoundingBox();
  if (!bounds.has_value()) {
    return std::nullopt;
  }
  return bounds->TransformBounds(transform);
}

std::optional<std::pair<Point, Point>> Path::GetMinMaxCoveragePoints() const {
  if (linears_.empty() && quads_.empty() && cubics_.empty()) {
    return std::nullopt;
  }

  std::optional<Point> min, max;

  auto clamp = [&min, &max](const Point& point) {
    if (min.has_value()) {
      min = min->Min(point);
    } else {
      min = point;
    }

    if (max.has_value()) {
      max = max->Max(point);
    } else {
      max = point;
    }
  };

  for (const auto& linear : linears_) {
    clamp(linear.p1);
    clamp(linear.p2);
  }

  for (const auto& quad : quads_) {
    for (const Point& point : quad.Extrema()) {
      clamp(point);
    }
  }

  for (const auto& cubic : cubics_) {
    for (const Point& point : cubic.Extrema()) {
      clamp(point);
    }
  }

  if (!min.has_value() || !max.has_value()) {
    return std::nullopt;
  }

  return std::make_pair(min.value(), max.value());
}

}  // namespace impeller
