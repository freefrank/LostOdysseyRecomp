#include "model_preview.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace modding::model_preview
{
namespace
{
using Vec3 = std::array<float, 3>;
Vec3 Add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 Sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 Scale(const Vec3 &v, float s) { return {v[0] * s, v[1] * s, v[2] * s}; }
float Dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Cross(const Vec3 &a, const Vec3 &b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 Normalize(const Vec3 &v)
{
    const float length = std::sqrt(Dot(v, v));
    return length > 0 ? Scale(v, 1 / length) : Vec3{0, 1, 0};
}
bool Finite(const Vec3 &v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

// Drawn at twice the size and averaged down: cheap anti-aliasing.
constexpr uint32_t kSupersample = 2;
constexpr float kAmbient = 0.42f, kDiffuse = 0.62f, kUntextured = 0.72f;
// Three-quarter view: 35 degrees from +x towards +z, 19 degrees above.
constexpr float kAzimuth = 0.61f, kElevation = 0.33f;
// Share of the frame the model's longer side fills.
constexpr float kFill = 0.9f;
}

Image Render(const gltf::Mesh &mesh, const std::vector<Texture> &textures, uint32_t size)
{
    const uint32_t w = size * kSupersample, h = w;
    std::vector<float> color(size_t(w) * h * 3);
    for (uint32_t y = 0; y < h; ++y)
    {
        const float t = float(y) / float(h - 1), grey = 0.36f + (0.17f - 0.36f) * t;
        for (uint32_t x = 0; x < w; ++x)
        {
            float *p = &color[(size_t(y) * w + x) * 3];
            p[0] = grey; p[1] = grey * 1.02f; p[2] = grey * 1.08f;
        }
    }
    std::vector<float> depth(size_t(w) * h, 0.0f); // 1/z of the nearest surface; 0 = nothing

    const auto &positions = mesh.positions;
    Vec3 low{1e30f, 1e30f, 1e30f}, high{-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (const auto index : mesh.indices)
    {
        if (index >= positions.size() || !Finite(positions[index])) continue;
        for (int k = 0; k < 3; ++k)
        {
            low[k] = std::min(low[k], positions[index][k]);
            high[k] = std::max(high[k], positions[index][k]);
        }
        any = true;
    }
    if (any)
    {
        const Vec3 center = Scale(Add(low, high), 0.5f);
        float radius = 0;
        for (const auto index : mesh.indices)
            if (index < positions.size() && Finite(positions[index]))
                radius = std::max(radius, Dot(Sub(positions[index], center), Sub(positions[index], center)));
        radius = std::max(std::sqrt(radius), 1e-3f);
        const Vec3 toCamera{std::cos(kElevation) * std::cos(kAzimuth), std::sin(kElevation),
                            std::cos(kElevation) * std::sin(kAzimuth)};
        const Vec3 eye = Add(center, Scale(toCamera, radius * 4));
        const Vec3 forward = Scale(toCamera, -1), right = Normalize(Cross(forward, {0, 1, 0})), up = Cross(right, forward);
        const Vec3 light = Normalize(Add(Add(toCamera, Scale(up, 0.8f)), Scale(right, -0.6f)));

        // View space: x/z, y/z and 1/z; 1/z = 0 marks a vertex behind the eye.
        std::vector<std::array<float, 3>> view(positions.size(), {0, 0, 0});
        std::vector<float> shade(positions.size(), 1.0f);
        for (size_t i = 0; i < positions.size(); ++i)
        {
            if (!Finite(positions[i])) continue;
            const Vec3 q = Sub(positions[i], eye);
            const float z = Dot(q, forward);
            if (z > 1e-5f) view[i] = {Dot(q, right) / z, Dot(q, up) / z, 1 / z};
            if (i < mesh.normals.size()) shade[i] = kAmbient + kDiffuse * std::fabs(Dot(Normalize(mesh.normals[i]), light));
        }
        // Fit what is drawn, not the bounding sphere: a 2D zoom and pan of the image plane.
        float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
        for (const auto index : mesh.indices)
            if (index < view.size() && view[index][2] > 0)
            {
                x0 = std::min(x0, view[index][0]); x1 = std::max(x1, view[index][0]);
                y0 = std::min(y0, view[index][1]); y1 = std::max(y1, view[index][1]);
            }
        const float half = std::max({x1 - x0, y1 - y0, 1e-6f}) / 2;
        const float scale = kFill * float(w) / 2 / half, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        auto screenX = [&](float x) { return float(w) / 2 + (x - cx) * scale; };
        auto screenY = [&](float y) { return float(h) / 2 - (y - cy) * scale; };
        const auto *uvs = mesh.uvs.empty() ? nullptr : &mesh.uvs[0];

        for (const auto &section : mesh.sections)
        {
            const Texture *texture = section.material < textures.size() ? &textures[section.material] : nullptr;
            if (texture && (!texture->pixels || !texture->width || !texture->height || !texture->channels)) texture = nullptr;
            const uint64_t end = std::min<uint64_t>(uint64_t(section.firstIndex) + section.indexCount, mesh.indices.size());
            for (uint64_t t = section.firstIndex; t + 3 <= end; t += 3)
            {
                const uint32_t v[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
                if (v[0] >= view.size() || v[1] >= view.size() || v[2] >= view.size()) continue;
                if (view[v[0]][2] <= 0 || view[v[1]][2] <= 0 || view[v[2]][2] <= 0) continue;
                float sx[3], sy[3], iz[3];
                for (int k = 0; k < 3; ++k)
                {
                    sx[k] = screenX(view[v[k]][0]); sy[k] = screenY(view[v[k]][1]); iz[k] = view[v[k]][2];
                }
                const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sy[1] - sy[0]) * (sx[2] - sx[0]);
                if (!(std::fabs(area) > 1e-9f)) continue;
                const int minX = std::max(0, int(std::floor(std::min({sx[0], sx[1], sx[2]}))));
                const int maxX = std::min(int(w) - 1, int(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
                const int minY = std::max(0, int(std::floor(std::min({sy[0], sy[1], sy[2]}))));
                const int maxY = std::min(int(h) - 1, int(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
                for (int py = minY; py <= maxY; ++py)
                    for (int px = minX; px <= maxX; ++px)
                    {
                        const float x = float(px) + 0.5f, y = float(py) + 0.5f;
                        // Barycentric weights from the signed sub-areas; no back-face culling.
                        const float b0 = ((sx[2] - sx[1]) * (y - sy[1]) - (sy[2] - sy[1]) * (x - sx[1])) / area;
                        const float b1 = ((sx[0] - sx[2]) * (y - sy[2]) - (sy[0] - sy[2]) * (x - sx[2])) / area;
                        const float b2 = 1 - b0 - b1;
                        if (b0 < 0 || b1 < 0 || b2 < 0) continue;
                        const float inverse = b0 * iz[0] + b1 * iz[1] + b2 * iz[2];
                        const size_t pixel = size_t(py) * w + size_t(px);
                        if (inverse <= depth[pixel]) continue;
                        // Perspective-correct weights for the attributes.
                        const float p0 = b0 * iz[0] / inverse, p1 = b1 * iz[1] / inverse, p2 = 1 - p0 - p1;
                        float rgb[3] = {kUntextured, kUntextured, kUntextured};
                        if (texture && uvs && v[0] < uvs->size() && v[1] < uvs->size() && v[2] < uvs->size())
                        {
                            const auto &a = (*uvs)[v[0]], &b = (*uvs)[v[1]], &c = (*uvs)[v[2]];
                            const float u = p0 * a[0] + p1 * b[0] + p2 * c[0], vv = p0 * a[1] + p1 * b[1] + p2 * c[1];
                            if (std::isfinite(u) && std::isfinite(vv))
                            {
                                const auto tx = std::min(texture->width - 1, uint32_t((u - std::floor(u)) * float(texture->width)));
                                const auto ty = std::min(texture->height - 1, uint32_t((vv - std::floor(vv)) * float(texture->height)));
                                const uint8_t *texel = texture->pixels + (size_t(ty) * texture->width + tx) * texture->channels;
                                if (texture->mask && texture->channels == 4 && texel[3] < 128) continue;
                                for (int k = 0; k < 3; ++k) rgb[k] = float(texel[texture->channels >= 3 ? k : 0]) / 255;
                            }
                        }
                        const float lit = p0 * shade[v[0]] + p1 * shade[v[1]] + p2 * shade[v[2]];
                        float *out = &color[pixel * 3];
                        for (int k = 0; k < 3; ++k) out[k] = rgb[k] * lit;
                        depth[pixel] = inverse;
                    }
            }
        }
    }

    Image image{size, size, std::vector<uint8_t>(size_t(size) * size * 3)};
    for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x)
            for (int k = 0; k < 3; ++k)
            {
                float sum = 0;
                for (uint32_t dy = 0; dy < kSupersample; ++dy)
                    for (uint32_t dx = 0; dx < kSupersample; ++dx)
                        sum += color[((size_t(y) * kSupersample + dy) * w + size_t(x) * kSupersample + dx) * 3 + k];
                const float value = sum / float(kSupersample * kSupersample);
                image.rgb[(size_t(y) * size + x) * 3 + k] = uint8_t(std::clamp(value, 0.0f, 1.0f) * 255 + 0.5f);
            }
    return image;
}
}
