#include "Labs/5-Visualization/tasks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using VCX::Labs::Common::ImageRGB;
namespace VCX::Labs::Visualization {

    // Helper to mix colors
    static glm::vec4 MixColor(glm::vec4 const & c1, glm::vec4 const & c2, float t) {
        return c1 * (1.0f - t) + c2 * t;
    }

    struct CoordinateStates {
        struct Dimension {
            std::string                       name;
            std::string                       unit;
            std::function<float(Car const &)> getter;
            float                             minVal    = 0.0f;
            float                             maxVal    = 1.0f;
            float                             filterMin = 0.0f;
            float                             filterMax = 1.0f;

            float Normalize(float val) const {
                if (maxVal == minVal) return 0.5f;
                return (val - minVal) / (maxVal - minVal);
            }

            float Denormalize(float t) const {
                return minVal + t * (maxVal - minVal);
            }
        };

        std::vector<Dimension>   dimensions;
        std::vector<Car> const & data;

        int   activeDimIndex = 0;
        int   dragDimIndex   = -1;
        int   dragMode       = 0; // 0: None, 1: Move, 2: ResizeMin, 3: ResizeMax, 4: Create
        float dragStartMin   = 0.0f;
        float dragStartMax   = 0.0f;
        float dragStartVal   = 0.0f;

        glm::vec4 colorStart = glm::vec4(0.275f, 0.510f, 0.706f, 1.0f); // Steelblue
        glm::vec4 colorEnd   = glm::vec4(0.647f, 0.165f, 0.165f, 1.0f); // Brown
        glm::vec4 colorGray  = glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);       // Gray

        CoordinateStates(std::vector<Car> const & data):
            data(data) {
            dimensions = {
                {    "cylinders",       "", [](Car const & c) { return (float) c.cylinders; } },
                { "displacement", " sq in",      [](Car const & c) { return c.displacement; } },
                {       "weight",   " lbs",            [](Car const & c) { return c.weight; } },
                {   "horsepower",    " hp",        [](Car const & c) { return c.horsepower; } },
                { "acceleration",   " sec",      [](Car const & c) { return c.acceleration; } },
                {      "mileage",   " mpg",           [](Car const & c) { return c.mileage; } },
                {         "year",       "",      [](Car const & c) { return (float) c.year; } }
            };

            if (data.empty()) return;

            for (auto & dim : dimensions) {
                dim.minVal = dim.getter(data[0]);
                dim.maxVal = dim.getter(data[0]);
                for (auto const & car : data) {
                    float val  = dim.getter(car);
                    dim.minVal = fmin(val, dim.minVal);
                    dim.maxVal = fmax(val, dim.maxVal);
                }
                float range = dim.maxVal - dim.minVal;
                if (range == 0) range = 1.0f;
                // Add slight padding to min/max so points aren't exactly on edge
                dim.minVal -= range * 0.05f;
                dim.maxVal += range * 0.05f;
                dim.filterMin = dim.minVal;
                dim.filterMax = dim.maxVal;
            }
        }

        const std::pair<float, float> lefttop     = { 0.05f, 0.05f };
        const std::pair<float, float> rightbottom = { 0.8f, 0.6f };

        float GetValueFromY(Dimension const & dim, float y) {
            float t = (y - rightbottom.second) / (lefttop.second - rightbottom.second);
            return dim.Denormalize(t);
        }

        bool update(InteractProxy const & proxy) {
            if (! proxy.IsHovering()) return false;

            // Update Mouse Cursor
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            glm::vec2 pos = proxy.MousePos();
            for (std::size_t i = 0; i < dimensions.size(); ++i) {
                auto const & dim = dimensions[i];
                float const  x   = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);

                if (std::abs(pos.x - x) < 0.04f) {
                    float const range = dim.maxVal - dim.minVal;
                    float const y1    = rightbottom.second + (dim.filterMin - dim.minVal) / range * (lefttop.second - rightbottom.second);
                    float const y2    = rightbottom.second + (dim.filterMax - dim.minVal) / range * (lefttop.second - rightbottom.second);
                    float const yMin  = std::min(y1, y2);
                    float const yMax  = std::max(y1, y2);

                    if (std::abs(pos.y - yMin) < 0.01f || std::abs(pos.y - yMax) < 0.01f)
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    else if (pos.y > yMin && pos.y < yMax)
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    else if (pos.y > lefttop.second && pos.y < rightbottom.second)
                        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
                }
            }

            // Double Click Reset
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                for (std::size_t i = 0; i < dimensions.size(); ++i) {
                    float const x = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);
                    if (std::abs(pos.x - x) < 0.05f) {
                        dimensions[i].filterMin = dimensions[i].minVal;
                        dimensions[i].filterMax = dimensions[i].maxVal;
                        activeDimIndex          = i;
                        return true;
                    }
                }
            }

            if (proxy.IsDragging(true)) {
                if (dragMode == 0) {
                    // Initialize Drag
                    glm::vec2 startPos = proxy.DraggingStartPoint(true);
                    for (std::size_t i = 0; i < dimensions.size(); ++i) {
                        float const x = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);
                        if (std::abs(startPos.x - x) < 0.05f) {
                            dragDimIndex   = i;
                            activeDimIndex = i;
                            auto & dim     = dimensions[i];

                            float val         = GetValueFromY(dim, startPos.y);
                            float range       = dim.maxVal - dim.minVal;
                            float tMin        = dim.Normalize(dim.filterMin);
                            float screenY_Min = rightbottom.second + tMin * (lefttop.second - rightbottom.second);
                            float tMax        = dim.Normalize(dim.filterMax);
                            float screenY_Max = rightbottom.second + tMax * (lefttop.second - rightbottom.second);

                            if (std::abs(startPos.y - screenY_Max) < 0.01f) {
                                dragMode = 3; // Resize Max (Top)
                            } else if (std::abs(startPos.y - screenY_Min) < 0.01f) {
                                dragMode = 2; // Resize Min (Bottom)
                            } else if (startPos.y > screenY_Max && startPos.y < screenY_Min) {
                                dragMode     = 1; // Move
                                dragStartVal = val;
                            } else {
                                dragMode     = 4; // Create
                                dragStartVal = val;
                            }

                            dragStartMin = dim.filterMin;
                            dragStartMax = dim.filterMax;
                            break;
                        }
                    }
                }

                if (dragMode != 0 && dragDimIndex >= 0 && dragDimIndex < dimensions.size()) {
                    auto & dim     = dimensions[dragDimIndex];
                    float  currVal = GetValueFromY(dim, pos.y);

                    if (dragMode == 1) { // Move
                        float delta  = currVal - dragStartVal;
                        float range  = dragStartMax - dragStartMin;
                        float newMin = dragStartMin + delta;
                        float newMax = dragStartMax + delta;

                        if (newMin < dim.minVal) {
                            newMin = dim.minVal;
                            newMax = newMin + range;
                        }
                        if (newMax > dim.maxVal) {
                            newMax = dim.maxVal;
                            newMin = newMax - range;
                        }

                        dim.filterMin = newMin;
                        dim.filterMax = newMax;
                    } else if (dragMode == 2) { // Resize Min (Bottom handle)
                        dim.filterMin = std::min(std::max(currVal, dim.minVal), dim.filterMax);
                    } else if (dragMode == 3) { // Resize Max (Top handle)
                        dim.filterMax = std::max(std::min(currVal, dim.maxVal), dim.filterMin);
                    } else if (dragMode == 4) { // Create
                        dim.filterMin = std::max(dim.minVal, std::min(dragStartVal, currVal));
                        dim.filterMax = std::min(dim.maxVal, std::max(dragStartVal, currVal));
                    }
                    return true;
                }
            } else {
                dragMode     = 0;
                dragDimIndex = -1;
            }

            if (! proxy.IsClicking(true))
                return false;

            for (std::size_t i = 0; i < dimensions.size(); ++i) {
                float const x = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);
                if (std::abs(pos.x - x) < 0.05f) {
                    if (activeDimIndex != i) {
                        activeDimIndex = i;
                        return true;
                    }
                    break;
                }
            }
            return false;
        }

        void PaintAxes(Common::ImageRGB & input) {
            std::stringstream ss;
            for (std::size_t i = 0; i < dimensions.size(); ++i) {
                auto const & dim = dimensions[i];

                // Calculate the x position
                float const x = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);

                // calculate the gray vertical line
                DrawLine(input, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), { x, lefttop.second }, { x, rightbottom.second }, 1.0f);

                // print dimension name
                PrintText(input, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), { x, lefttop.second - 0.03f }, 0.02f, dim.name);

                // print max value
                ss.str("");
                ss << std::fixed << std::setprecision(1) << dim.maxVal << dim.unit;
                PrintText(input, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), { x, lefttop.second - 0.01f }, 0.015f, ss.str());

                // print min value
                ss.str("");
                ss << std::fixed << std::setprecision(1) << dim.minVal << dim.unit;
                PrintText(input, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), { x, rightbottom.second + 0.01f }, 0.015f, ss.str());
            }
        }

        void PaintFilterBars(Common::ImageRGB & input) {
            std::stringstream ss;
            for (std::size_t i = 0; i < dimensions.size(); ++i) {
                auto const & dim = dimensions[i];
                // 计算当前维度在 X 方向上的位置
                float const x = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);

                // 计算 filter 区间对应的 Y 坐标
                float const range = dim.maxVal - dim.minVal;
                float const yMin  = rightbottom.second + (dim.filterMin - dim.minVal) / range * (lefttop.second - rightbottom.second);
                float const yMax  = rightbottom.second + (dim.filterMax - dim.minVal) / range * (lefttop.second - rightbottom.second);

                // 绘制 filter 区间（灰色矩形）
                glm::vec4 barColor = colorGray;
                if (i == activeDimIndex) {
                    float const midVal = (dim.filterMin + dim.filterMax) * 0.5f;
                    barColor           = MixColor(colorStart, colorEnd, dim.Normalize(midVal));
                }
                DrawFilledRect(input, barColor, { x - 0.005f, yMin - 0.002f }, { 0.01f, yMax - yMin + 0.004f });
                DrawRect(input, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), { x - 0.005f, yMin - 0.002f }, { 0.01f, yMax - yMin + 0.004f }, 1.0f);

                // 在 filter 区间右侧显示 filter 的最大值
                ss.str("");
                ss << std::fixed << std::setprecision(1) << dim.filterMax << dim.unit;
                PrintText(input, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), { x + 0.04f, yMax }, 0.015f, ss.str());

                // 在 filter 区间右侧显示 filter 的最小值
                ss.str("");
                ss << std::fixed << std::setprecision(1) << dim.filterMin << dim.unit;
                PrintText(input, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), { x + 0.04f, yMin }, 0.015f, ss.str());
            }
        }

        void PaintContextLines(Common::ImageRGB & input) {
            for (auto const & car : data) {
                for (std::size_t i = 0; i < dimensions.size() - 1; ++i) {
                    auto const & dim1 = dimensions[i];
                    auto const & dim2 = dimensions[i + 1];

                    float const x1 = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);
                    float const x2 = lefttop.first + (rightbottom.first - lefttop.first) * (i + 1) / (dimensions.size() - 1);

                    float const y1 = rightbottom.second + dim1.Normalize(dim1.getter(car)) * (lefttop.second - rightbottom.second);
                    float const y2 = rightbottom.second + dim2.Normalize(dim2.getter(car)) * (lefttop.second - rightbottom.second);

                    DrawLine(input, glm::vec4(0.86f, 0.86f, 0.86f, 1.0f), { x1, y1 }, { x2, y2 }, 0.75f);
                }
            }
        }

        void PaintActiveLines(Common::ImageRGB & input) {
            for (auto const & car : data) {
                bool pass = true;
                for (auto const & dim : dimensions) {
                    if (dim.getter(car) < dim.filterMin || dim.getter(car) > dim.filterMax) {
                        pass = false;
                        break;
                    }
                }
                if (! pass)
                    continue;

                float const t = dimensions[activeDimIndex].Normalize(dimensions[activeDimIndex].getter(car));
                // calculate color
                glm::vec4 color = MixColor(colorStart, colorEnd, t);

                for (std::size_t i = 0; i < dimensions.size() - 1; ++i) {
                    auto const & dim1 = dimensions[i];
                    auto const & dim2 = dimensions[i + 1];

                    float const x1 = lefttop.first + (rightbottom.first - lefttop.first) * i / (dimensions.size() - 1);
                    float const x2 = lefttop.first + (rightbottom.first - lefttop.first) * (i + 1) / (dimensions.size() - 1);

                    float const y1 = rightbottom.second + dim1.Normalize(dim1.getter(car)) * (lefttop.second - rightbottom.second);
                    float const y2 = rightbottom.second + dim2.Normalize(dim2.getter(car)) * (lefttop.second - rightbottom.second);

                    DrawLine(input, color, { x1, y1 }, { x2, y2 }, 0.85f);
                }
            }
        }

        bool paint(Common::ImageRGB & input) {
            SetBackGround(input, glm::vec4(1.0f));
            PaintContextLines(input);
            PaintActiveLines(input);
            PaintAxes(input);
            PaintFilterBars(input);
            return true;
        }
    };

    bool PaintParallelCoordinates(Common::ImageRGB & input, InteractProxy const & proxy, std::vector<Car> const & data, bool force) {
        static CoordinateStates states(data);
        bool                    changed = states.update(proxy);
        if (! force && ! changed)
            return false;
        states.paint(input);
        return true;
    }

    glm::vec2 GetVector(VectorField2D const & field, glm::vec2 point) {
        float x = point.x, y = point.y;
        int   x0 = static_cast<int>(std::floor(x));
        int   y0 = static_cast<int>(std::floor(y));
        int   x1 = x0 + 1;
        int   y1 = y0 + 1;

        float tx = x - x0;
        float ty = y - y0;

        auto clampX = [&](int val) { return std::clamp(val, 0, (int) field.size.first - 1); };
        auto clampY = [&](int val) { return std::clamp(val, 0, (int) field.size.second - 1); };

        glm::vec2 v00 = field.At(clampX(x0), clampY(y0));
        glm::vec2 v10 = field.At(clampX(x1), clampY(y0));
        glm::vec2 v01 = field.At(clampX(x0), clampY(y1));
        glm::vec2 v11 = field.At(clampX(x1), clampY(y1));

        return glm::mix(glm::mix(v00, v10, tx), glm::mix(v01, v11, tx), ty);
    }

    glm::vec3 GetNoise(ImageRGB const & noise, glm::vec2 point) {
        float x = point.x, y = point.y;
        int   x0 = static_cast<int>(std::floor(x));
        int   y0 = static_cast<int>(std::floor(y));
        int   x1 = x0 + 1;
        int   y1 = y0 + 1;

        float tx = x - x0;
        float ty = y - y0;

        auto clampX = [&](int val) { return std::clamp(val, 0, (int) noise.GetSizeX() - 1); };
        auto clampY = [&](int val) { return std::clamp(val, 0, (int) noise.GetSizeY() - 1); };

        glm::vec3 c00 = noise.At(clampX(x0), clampY(y0));
        glm::vec3 c10 = noise.At(clampX(x1), clampY(y0));
        glm::vec3 c01 = noise.At(clampX(x0), clampY(y1));
        glm::vec3 c11 = noise.At(clampX(x1), clampY(y1));

        return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
    }

    const float            ds = 1.0f;
    std::vector<glm::vec2> compute_integral_curve(VectorField2D const & field, glm::vec2 point, int L) {
        std::vector<glm::vec2> curve;
        curve.reserve(L * 2 + 1);
        curve.push_back(point);

        auto is_valid = [&](glm::vec2 const & p) {
            return p.x >= 0 && p.x < field.size.first && p.y >= 0 && p.y < field.size.second;
        };

        // Forward
        auto now_point  = point;
        auto now_vector = GetVector(field, now_point);
        if (glm::length(now_vector) > 1e-5f) now_vector = glm::normalize(now_vector);
        else now_vector = glm::vec2(0, 0);

        for (int s = 0; s < L; s++) {
            if (glm::length(now_vector) < 1e-5f) break;
            now_point += now_vector * ds;
            if (! is_valid(now_point)) break;
            curve.push_back(now_point);
            now_vector = GetVector(field, now_point);
            if (glm::length(now_vector) > 1e-5f) now_vector = glm::normalize(now_vector);
            else now_vector = glm::vec2(0, 0);
        }

        // Backward
        now_point  = point;
        now_vector = GetVector(field, now_point);
        if (glm::length(now_vector) > 1e-5f) now_vector = glm::normalize(now_vector);
        else now_vector = glm::vec2(0, 0);

        for (int s = 0; s < L; s++) {
            if (glm::length(now_vector) < 1e-5f) break;
            now_point -= now_vector * ds;
            if (! is_valid(now_point)) break;
            curve.push_back(now_point);
            now_vector = GetVector(field, now_point);
            if (glm::length(now_vector) > 1e-5f) now_vector = glm::normalize(now_vector);
            else now_vector = glm::vec2(0, 0);
        }
        return curve;
    }

    void LIC(ImageRGB & output, Common::ImageRGB const & noise, VectorField2D const & field, int const & step) {
        for (std::size_t x = 0; x < field.size.first; ++x) {
            for (std::size_t y = 0; y < field.size.second; ++y) {
                glm::vec2 point(x + 0.5f, y + 0.5f);
                auto curve = compute_integral_curve(field, point, step / 2);
                glm::vec3 sum(0.0f);
                for (auto const & p : curve) {
                    sum += GetNoise(noise, p);
                }
                output.At(x, y) = sum / static_cast<float>(curve.size());
            }
        }
    }
}; // namespace VCX::Labs::Visualization