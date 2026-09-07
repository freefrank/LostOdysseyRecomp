#pragma once

// Camera-only CPU reference math. No history ownership, jitter inference,
// object motion, disocclusion test, or renderer integration is provided here.
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace gpu::temporal
{
using Matrix = std::array<double, 16>; // Row-major storage, row vectors: clip = world * VP.
using Vector = std::array<double, 4>;

struct Viewport
{
    double x, y, width, height; // Host raster edges, Y down; dimensions >= 1.
    // Explicit guest-NDC -> host-NDC transform before the D3D viewport.
    // Only +/-1 Y scale is supported. X scale is 1. No other XY offsets.
    double ndcYSign = 1;
    double halfPixelNdcX = 0;
    double halfPixelNdcY = 0;
};

struct Sample
{
    // Continuous host raster position: integer texel (i,j) has center (i+.5,j+.5).
    // Do not pass integer texel indices. Current and previous viewport may differ.
    double x, y;
    double depth; // Host reversed depth d=1-z_clip/w. Zero is rejected as clear/background.
};

inline bool Finite(const Matrix& m)
{
    return std::all_of(m.begin(), m.end(), [](double x) { return std::isfinite(x); });
}

inline Vector Transform(const Vector& v, const Matrix& m)
{
    Vector result{};
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) result[j] += v[i] * m[i * 4 + j];
    return result;
}

inline std::optional<Matrix> Inverse(const Matrix& m)
{
    if (!Finite(m)) return {};
    double a[4][8]{}, scale[4]{};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            a[i][j] = m[i * 4 + j];
            scale[i] = std::max(scale[i], std::abs(a[i][j]));
        }
        if (scale[i] == 0) return {};
        a[i][i + 4] = 1;
    }
    for (int col = 0; col < 4; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::abs(a[row][col]) / scale[row] > std::abs(a[pivot][col]) / scale[pivot]) pivot = row;
        // Scaled pivot and condition limits deliberately reject numerically unsafe inputs.
        if (std::abs(a[pivot][col]) / scale[pivot] <= 1e-12) return {};
        if (pivot != col)
        {
            for (int j = 0; j < 8; ++j) std::swap(a[col][j], a[pivot][j]);
            std::swap(scale[col], scale[pivot]);
        }
        const double divisor = a[col][col];
        for (double& x : a[col]) x /= divisor;
        for (int row = 0; row < 4; ++row)
        {
            if (row == col) continue;
            const double factor = a[row][col];
            for (int j = 0; j < 8; ++j) a[row][j] -= factor * a[col][j];
        }
    }
    Matrix inverse{};
    double norm = 0, inverseNorm = 0;
    for (int i = 0; i < 4; ++i)
    {
        double rowNorm = 0, inverseRowNorm = 0;
        for (int j = 0; j < 4; ++j)
        {
            inverse[i * 4 + j] = a[i][j + 4];
            rowNorm += std::abs(m[i * 4 + j]);
            inverseRowNorm += std::abs(a[i][j + 4]);
        }
        norm = std::max(norm, rowNorm);
        inverseNorm = std::max(inverseNorm, inverseRowNorm);
    }
    if (!Finite(inverse) || !std::isfinite(norm * inverseNorm) || norm * inverseNorm > 1e12) return {};
    return inverse;
}

class Camera
{
public:
    static std::optional<Camera> Create(const Matrix& viewProjection, const Viewport& viewport)
    {
        const auto& v = viewport;
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.width) || !std::isfinite(v.height)
            || v.width < 1 || v.height < 1 || !std::isfinite(v.x + v.width) || !std::isfinite(v.y + v.height)
            || (v.ndcYSign != 1 && v.ndcYSign != -1)
            || !std::isfinite(v.halfPixelNdcX) || !std::isfinite(v.halfPixelNdcY)) return {};
        auto inverse = Inverse(viewProjection);
        if (!inverse) return {};
        return Camera(viewProjection, *inverse, viewport);
    }
    const Matrix& VP() const { return vp_; }
    const Matrix& InverseVP() const { return inverse_; }
    const Viewport& Raster() const { return viewport_; }
private:
    Camera(const Matrix& vp, const Matrix& inverse, const Viewport& viewport)
        : vp_(vp), inverse_(inverse), viewport_(viewport) {}
    Matrix vp_, inverse_;
    Viewport viewport_;
};

enum class Rejection { None, InvalidSample, CurrentOutside, InvalidWorldW, InvalidPreviousW, PreviousDepth, PreviousOutside };
struct Result
{
    Rejection rejection = Rejection::InvalidSample;
    Sample previous{};
    Vector world{}; // Normalized homogeneous world coordinate (w=1), only valid on success.
    explicit operator bool() const { return rejection == Rejection::None; }
};

inline bool InsideCenters(double x, double y, const Viewport& v)
{
    // Conservative center domain: never asks history sampling to reach beyond edge texels.
    return std::isfinite(x) && std::isfinite(y) && x >= v.x + .5 && y >= v.y + .5
        && x <= v.x + v.width - .5 && y <= v.y + v.height - .5;
}

inline bool ValidW(const Vector& v)
{
    double magnitude = 0;
    for (double x : v)
    {
        if (!std::isfinite(x)) return false;
        magnitude = std::max(magnitude, std::abs(x));
    }
    // This reference supports positive clip-W projection, with a relative divide guard.
    return magnitude > 0 && v[3] > 1e-12 * magnitude;
}

inline Result Reproject(const Sample& sample, const Camera& current, const Camera& previous)
{
    auto reject = [](Rejection why) { Result r; r.rejection = why; return r; };
    if (!std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.depth)
        || sample.depth <= 0 || sample.depth > 1) return reject(Rejection::InvalidSample);
    const auto& c = current.Raster();
    const auto& p = previous.Raster();
    if (!InsideCenters(sample.x, sample.y, c)) return reject(Rejection::CurrentOutside);
    const double nx = 2 * ((sample.x - c.x) / c.width) - 1 - c.halfPixelNdcX;
    const double ny = (1 - 2 * ((sample.y - c.y) / c.height) - c.halfPixelNdcY) / c.ndcYSign;
    Vector world = Transform({nx, ny, 1 - sample.depth, 1}, current.InverseVP());
    if (!ValidW(world)) return reject(Rejection::InvalidWorldW);
    const double worldW = world[3];
    for (double& x : world) x /= worldW;
    const Vector clip = Transform(world, previous.VP());
    if (!ValidW(clip)) return reject(Rejection::InvalidPreviousW);
    Sample output{
        p.x + (clip[0] / clip[3] + p.halfPixelNdcX + 1) * .5 * p.width,
        p.y + (1 - (p.ndcYSign * clip[1] / clip[3] + p.halfPixelNdcY)) * .5 * p.height,
        1 - clip[2] / clip[3]};
    if (!std::isfinite(output.depth) || output.depth <= 0 || output.depth > 1) return reject(Rejection::PreviousDepth);
    if (!InsideCenters(output.x, output.y, p)) return reject(Rejection::PreviousOutside);
    return {Rejection::None, output, world};
}
} // namespace gpu::temporal
