#include "Labs/1-Drawing2D/tasks.h"
#include <random>
#include <spdlog/spdlog.h>

using VCX::Labs::Common::ImageRGB;

namespace VCX::Labs::Drawing2D {
    /******************* 1.Image Dithering *****************/
    void DitheringThreshold(
        ImageRGB &       output,
        ImageRGB const & input) {
        for (std::size_t x = 0; x < input.GetSizeX(); ++x)
            for (std::size_t y = 0; y < input.GetSizeY(); ++y) {
                glm::vec3 color = input.At(x, y);
                output.At(x, y) = {
                    color.r > 0.5 ? 1 : 0,
                    color.g > 0.5 ? 1 : 0,
                    color.b > 0.5 ? 1 : 0,
                };
            }
    }

    void DitheringRandomUniform(
        ImageRGB &       output,
        ImageRGB const & input) {
        std::minstd_rand                      gen(0);
        std::uniform_real_distribution<float> dist(-0.5, 0.5);

        for (std::size_t x = 0; x < input.GetSizeX(); ++x)
            for (std::size_t y = 0; y < input.GetSizeY(); ++y) {
                glm::vec3 color = input.At(x, y) + dist(gen);
                output.At(x, y) = {
                    color.r > 0.5 ? 1 : 0,
                    color.g > 0.5 ? 1 : 0,
                    color.b > 0.5 ? 1 : 0,
                };
            }
        // your code here:
    }

    void DitheringRandomBlueNoise(
        ImageRGB &       output,
        ImageRGB const & input,
        ImageRGB const & noise) {
        float       r = 0, g = 0, b = 0;
        std::size_t lx = input.GetSizeX(), ly = input.GetSizeY();
        for (std::size_t x = 0; x < lx; ++x)
            for (std::size_t y = 0; y < ly; ++y) {
                r += noise.At(x, y).r;
                g += noise.At(x, y).g;
                b += noise.At(x, y).b;
            }
        r                 = r / (lx * ly);
        g                 = g / (lx * ly);
        b                 = b / (lx * ly);
        glm::vec3 average = glm::vec3 { r, g, b };
        for (std::size_t x = 0; x < lx; ++x)
            for (std::size_t y = 0; y < ly; ++y) {
                glm::vec3 n         = noise.At(x, y) - average;
                glm::vec3 perturbed = input.At(x, y) + n;

                output.At(x, y) = glm::vec3(
                    perturbed.r > 0.5f ? 1.0f : 0.0f,
                    perturbed.g > 0.5f ? 1.0f : 0.0f,
                    perturbed.b > 0.5f ? 1.0f : 0.0f);
            }
    }

    void DitheringOrdered(
        ImageRGB &       output,
        ImageRGB const & input) {
        // your code here:
        const glm::mat<3, 3, float> shake_matrix = {
            { 6.0f / 9, 8.0f / 9, 4.0f / 9 },
            { 1.0f / 9, 0.0f / 9, 3.0f / 9 },
            { 5.0f / 9, 2.0f / 9, 7.0f / 9 }
        };
        for (std::size_t x = 0; x < input.GetSizeX(); ++x)
            for (std::size_t y = 0; y < input.GetSizeY(); ++y) {
                for (std::size_t i = 0; i < 3; ++i) {
                    for (std::size_t j = 0; j < 3; ++j) {
                        output.At(x * 3 + i, y * 3 + j) = {
                            (input.At(x, y)).r > shake_matrix[i][j] ? 1 : 0,
                            (input.At(x, y)).g > shake_matrix[i][j] ? 1 : 0,
                            (input.At(x, y)).b > shake_matrix[i][j] ? 1 : 0,
                        };
                    }
                }
            }
    }

    void DitheringErrorDiffuse(
        ImageRGB &       output,
        ImageRGB const & input) {
        // 拷贝输入作为临时缓冲
        ImageRGB temp       = input;
        auto     modify_pos = [&temp](std::size_t x, std::size_t y, glm::vec3 error) {
            glm::vec3 now_color = temp.At(x, y);
            temp.At(x, y)       = { now_color + error };
        };
        for (std::size_t y = 0; y < input.GetSizeY(); ++y) {
            for (std::size_t x = 0; x < input.GetSizeX(); ++x) {
                glm::vec3 color      = temp.At(x, y);
                glm::vec3 fine_color = {
                    color.r > 0.5f ? 1.0f : 0.0f,
                    color.g > 0.5f ? 1.0f : 0.0f,
                    color.b > 0.5f ? 1.0f : 0.0f
                };

                output.At(x, y) = fine_color;
                glm::vec3 error = color - fine_color;
                bool      f1    = (x + 1 < input.GetSizeX());
                bool      f2    = (x > 0);
                bool      f3    = (y + 1 < input.GetSizeY());

                if (f1) modify_pos(x + 1, y, error * (7.0f / 16.0f));
                if (f2 && f3) modify_pos(x - 1, y + 1, error * (3.0f / 16.0f));
                if (f3) modify_pos(x, y + 1, error * (5.0f / 16.0f));
                if (f1 && f3) modify_pos(x + 1, y + 1, error * (1.0f / 16.0f));
            }
        }
    }

    /******************* 2.Image Filtering *****************/
    void Blur(
        ImageRGB &       output,
        ImageRGB const & input) {
        auto in_place = [&input](int x, int y) {
            return (x >= 0) && (x < input.GetSizeX()) && (y >= 0) && (y < input.GetSizeY());
        };
        for (std::size_t x = 0; x < input.GetSizeX(); ++x)
            for (std::size_t y = 0; y < input.GetSizeY(); ++y) {
                glm::vec3 color = { 0, 0, 0 };
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        glm::vec3 ret = in_place(x + i, y + j) ? input.At(x + i, y + j) : glm::vec3 { 0, 0, 0 };
                        color += ret / 9.0f;
                    }
                }
                output.At(x, y) = color;
            }
    }

    void Edge(
        ImageRGB &       output,
        ImageRGB const & input) {
        // your code here:
        const glm::mat<3, 3, float> x_kernel = {
            { -1, 0, 1 },
            { -2, 0, 2 },
            { -1, 0, 1 }
        };
        const glm::mat<3, 3, float> y_kernel = {
            {  1,  2,  1 },
            {  0,  0,  0 },
            { -1, -2, -1 }
        };

        auto in_place = [&input](int x, int y) {
            return (x >= 0) && (x < input.GetSizeX()) && (y >= 0) && (y < input.GetSizeY());
        };
        for (std::size_t x = 0; x < input.GetSizeX(); ++x)
            for (std::size_t y = 0; y < input.GetSizeY(); ++y) {
                glm::vec3 x_color = { 0, 0, 0 }, y_color = { 0, 0, 0 };
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        glm::vec3 ret = in_place(x + i, y + j) ? input.At(x + i, y + j) : glm::vec3 { 0, 0, 0 };
                        x_color += ret * x_kernel[i + 1][j + 1];
                        y_color += ret * y_kernel[i + 1][j + 1];
                    }
                }
                output.At(x, y) = sqrt((x_color * x_color) + (y_color * y_color));
            }
    }

    /******************* 3. Image Inpainting *****************/
    void Inpainting(
        ImageRGB &         output,
        ImageRGB const &   inputBack,
        ImageRGB const &   inputFront,
        const glm::ivec2 & offset) {
        output             = inputBack;
        std::size_t width  = inputFront.GetSizeX();
        std::size_t height = inputFront.GetSizeY();
        glm::vec3 * g      = new glm::vec3[width * height];
        memset(g, 0, sizeof(glm::vec3) * width * height);
        // set boundary condition
        for (std::size_t y = 0; y < height; ++y) {
            // set boundary for (0, y), your code: g[y * width] = ?
            // set boundary for (width - 1, y), your code: g[y * width + width - 1] = ?
            g[y * width]             = inputBack.At(offset[0], offset[1] + y) - inputFront.At(0, y);
            g[y * width + width - 1] = inputBack.At(offset[0] + width - 1, offset[1] + y) - inputFront.At(width - 1, y);
        }
        for (std::size_t x = 0; x < width; ++x) {
            // set boundary for (x, 0), your code: g[x] = ?
            // set boundary for (x, height - 1), your code: g[(height - 1) * width + x] = ?
            g[x]                        = inputBack.At(offset[0] + x, offset[1]) - inputFront.At(x, 0);
            g[(height - 1) * width + x] = inputBack.At(offset[0] + x, offset[1] + height - 1) - inputFront.At(x, height - 1);
        }

        // Jacobi iteration, solve Ag = b
        for (int iter = 0; iter < 8000; ++iter) {
            for (std::size_t y = 1; y < height - 1; ++y)
                for (std::size_t x = 1; x < width - 1; ++x) {
                    g[y * width + x] = (g[(y - 1) * width + x] + g[(y + 1) * width + x] + g[y * width + x - 1] + g[y * width + x + 1]);
                    g[y * width + x] = g[y * width + x] * glm::vec3(0.25);
                }
        }

        for (std::size_t y = 0; y < inputFront.GetSizeY(); ++y)
            for (std::size_t x = 0; x < inputFront.GetSizeX(); ++x) {
                glm::vec3 color                       = g[y * width + x] + inputFront.At(x, y);
                output.At(x + offset.x, y + offset.y) = color;
            }
        delete[] g;
    }

    /******************* 4. Line Drawing *****************/
    void DrawLine(
        ImageRGB &       canvas,
        glm::vec3 const  color,
        glm::ivec2 const p0,
        glm::ivec2 const p1) {
        // your code here:
        int dx = abs(p1.x - p0.x), dy = abs(p1.y - p0.y);
        int sx  = (p0.x < p1.x) ? 1 : -1;
        int sy  = (p0.y < p1.y) ? 1 : -1;
        int err = dx - dy;

        int x = p0.x, y = p0.y;
        while (true) {
            canvas.At(x, y) = color;
            if (x == p1.x && y == p1.y) break;
            int e2 = 2 * err;
            if (e2 > -dy) err -= dy, x += sx;
            if (e2 < dx) err += dx, y += sy;
        }
    }

    /******************* 5. Triangle Drawing *****************/
    void DrawTriangleFilled(
        ImageRGB &       canvas,
        glm::vec3 const  color,
        glm::ivec2 const p0,
        glm::ivec2 const p1,
        glm::ivec2 const p2) {
        // your code here:
        glm::ivec2 v0 = p0, v1 = p1, v2 = p2;
        if (v0.y > v1.y) std::swap(v0, v1);
        if (v0.y > v2.y) std::swap(v0, v2);
        if (v1.y > v2.y) std::swap(v1, v2);

        auto DrawScanLine = [&](int y, int x_start, int x_end) {
            if (x_start > x_end) std::swap(x_start, x_end);
            for (int x = x_start; x <= x_end; ++x)
                canvas.At(x, y) = color;
        };

        auto EdgeInterpolate = [](glm::ivec2 a, glm::ivec2 b, int y) -> int {
            if (a.y == b.y) return a.x;
            return a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
        };

        for (int y = v0.y; y <= v2.y; ++y) {
            int x_start, x_end;
            if (y < v1.y) {
                x_start = EdgeInterpolate(v0, v2, y);
                x_end   = EdgeInterpolate(v0, v1, y);
            } else {
                x_start = EdgeInterpolate(v0, v2, y);
                x_end   = EdgeInterpolate(v1, v2, y);
            }
            DrawScanLine(y, x_start, x_end);
        }
    }

    /******************* 6. Image Supersampling *****************/
    void Supersample(
        ImageRGB &       output,
        ImageRGB const & input,
        int              rate) {
        // your code here:
        int output_w = output.GetSizeX(), output_h = output.GetSizeY();
        int input_w = input.GetSizeX(), input_h = input.GetSizeY();
        int mid_w = output_w * rate, mid_h = output_h * rate;

        ImageRGB midput(output_w * rate, output_h * rate);
        float    r_rate = (input_w - 1) / ((float) (mid_w - 1));
        float    p, q;
        int      down, left, up, right;
        float    dx, dy;
        for (int i = 0; i < mid_w; i++) {
            for (int j = 0; j < mid_h; j++) {
                p = i * r_rate, q = j * r_rate;
                left = floor(p), down = floor(q);
                up = down + 1, right = left + 1;
                up    = std::min(up, input_h - 1);
                right = std::min(right, input_w - 1);
                dx = p - left, dy = q - down;
                midput.At(i, j) =
                    input.At(left, down) * (1 - dx) * (1 - dy) + // f00
                    input.At(right, down) * dx * (1 - dy) +      // f10
                    input.At(left, up) * (1 - dx) * dy +         // f01
                    input.At(right, up) * dx * dy;               // f11
            }
        }

        for (int x = 0; x < output_w; ++x) {
            for (int y = 0; y < output_h; ++y) {
                glm::vec3 ret = { 0, 0, 0 }, tmp;
                for (int i = 0; i < rate; i++) {
                    for (int j = 0; j < rate; j++) {
                        tmp = midput.At(x * rate + i, y * rate + j);
                        ret += tmp;
                    }
                }
                ret /= rate * rate;
                output.At(x, y) = ret;
            }
        }
    }

    /******************* 7. Bezier Curve *****************/
    // Note: Please finish the function [DrawLine] before trying this part.
    glm::vec2 CalculateBezierPoint(
        std::span<glm::vec2> points,
        float const          t) {
        // your code here:
        std::vector<glm::vec2> old_points(points.begin(), points.end());
        std::vector<glm::vec2> new_points;

        while (old_points.size() > 1) {
            new_points.clear();
            for (std::size_t i = 1; i < old_points.size(); i++) {
                new_points.push_back(old_points[i - 1] * (1.0f - t) + old_points[i] * t);
            }
            old_points = std::move(new_points);
        }

        return old_points.front();
    }
} // namespace VCX::Labs::Drawing2D