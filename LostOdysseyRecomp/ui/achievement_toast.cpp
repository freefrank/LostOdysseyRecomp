#include "achievement_toast.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// Layout (720p pixels, multiplied by the scale):
//   pill box:   height 60 (the outline is part of it), width = textLeft + widest text + right padding
//   ring disk:  diameter 52, concentric with the pill's left end
//   shading:    capsule distance field -> shadow / dark outline / light rim / gradient body
//
// Per-frame cost is kept low by baking everything that does not depend on the time once per
// (scale, text widths, icon): the pill (left cap, right cap, one middle column that is
// stretched), the ring disk, the lit ring + glow, the orb and the icon variants. A frame is a
// few memcpy/row fills, one blit of the circle sprite, the (alpha-blended) text and a
// premultiplied -> straight conversion of the touched columns.
namespace ui::achievement_toast {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDuration = 5.2;

// 720p metrics.
constexpr float kPillH = 60.f;
constexpr float kMargin = 18.f;     // shadow / glow room around the resting box
constexpr float kDiskR = 25.f;      // black disk radius
constexpr float kRingOuter = 23.2f;
constexpr float kRingInner = 17.2f;
constexpr float kGapHalf = 2.2f;    // half width of the cross-shaped gaps in the ring
constexpr float kCentreR = 16.f;   // orb / icon radius
constexpr float kHalo = 14.f;       // circle sprite room beyond the pill radius
constexpr float kTextLeft = 66.f;   // from the pill's left edge
constexpr float kPadRight = 30.f;
constexpr float kTitleDy = -10.f;   // text line centres relative to the pill centre
constexpr float kDetailDy = 11.f;
constexpr float kOvershoot = 1.025f;

constexpr double kPopEnd = 0.12;
constexpr double kGrowEnd = 0.46;
constexpr double kSettleEnd = 0.60;
constexpr double kPulseStart = 4.60, kPulseEnd = 4.80;
constexpr double kCollapseStart = 4.90;
constexpr double kFlipCentres[] = {1.0, 1.5, 2.5, 3.1, 3.9, 4.55};
constexpr double kFlipHalf = 0.05;

// ---- small helpers -----------------------------------------------------------------------------
inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline double Clamp01(double v) { return v < 0. ? 0. : (v > 1. ? 1. : v); }
inline float Cov(float d) { return Clamp01(0.5f - d); } // coverage from a signed distance in pixels
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline double Smooth(double u) { u = Clamp01(u); return u * u * (3. - 2. * u); }
inline double EaseOutQuad(double u) { u = Clamp01(u); return 1. - (1. - u) * (1. - u); }
inline double EaseOutCubic(double u) { u = Clamp01(u); double v = 1. - u; return 1. - v * v * v; }

// Straight colour 0..1 + coverage -> premultiplied RGBA8 (colour never exceeds alpha).
inline uint32_t PackPM(float r, float g, float b, float a)
{
    a = Clamp01(a);
    uint32_t A = (uint32_t)(a * 255.f + 0.5f);
    uint32_t R = std::min(A, (uint32_t)(Clamp01(r) * a * 255.f + 0.5f));
    uint32_t G = std::min(A, (uint32_t)(Clamp01(g) * a * 255.f + 0.5f));
    uint32_t B = std::min(A, (uint32_t)(Clamp01(b) * a * 255.f + 0.5f));
    return R | (G << 8) | (B << 16) | (A << 24);
}

struct PM { float r, g, b, a; }; // premultiplied float
inline PM Over(PM s, PM d) { float k = 1.f - s.a; return {s.r + d.r * k, s.g + d.g * k, s.b + d.b * k, s.a + d.a * k}; }
inline uint32_t Pack(PM p) { return PackPM(p.a > 0.f ? p.r / p.a : 0.f, p.a > 0.f ? p.g / p.a : 0.f, p.a > 0.f ? p.b / p.a : 0.f, p.a); }
inline PM FromStraight(float r, float g, float b, float a) { return {r * a, g * a, b * a, a}; }

// Premultiplied src-over, two channels per 32-bit lane; truncating so a channel never exceeds 255.
inline uint32_t OverPM(uint32_t s, uint32_t d)
{
    uint32_t ia = 255u - (s >> 24);
    uint32_t rb = (d & 0x00FF00FFu) * ia;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    uint32_t ag = ((d >> 8) & 0x00FF00FFu) * ia;
    ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
    return s + (rb | ag);
}
// f = 0..256
inline uint32_t MulPM(uint32_t p, uint32_t f)
{
    uint32_t rb = (((p & 0x00FF00FFu) * f) >> 8) & 0x00FF00FFu;
    uint32_t ag = (((p >> 8) & 0x00FF00FFu) * f) & 0xFF00FF00u;
    return rb | ag;
}
inline uint32_t TintPM(uint32_t p, uint32_t f) { return (MulPM(p, f) & 0x00FFFFFFu) | (p & 0xFF000000u); }
inline uint32_t Lerp8(uint32_t a, uint32_t b, uint32_t f) // f = 0..256, lanes
{
    uint32_t rb = ((((a & 0x00FF00FFu) * (256u - f)) + ((b & 0x00FF00FFu) * f)) >> 8) & 0x00FF00FFu;
    uint32_t ag = ((((a >> 8) & 0x00FF00FFu) * (256u - f)) + (((b >> 8) & 0x00FF00FFu) * f)) & 0xFF00FF00u;
    return rb | ag;
}

struct View { uint32_t* p; int w, h; };

// Source-over blit of a premultiplied sprite at integer position (clipped).
void BlitOver(View dst, const uint32_t* src, int sw, int sh, int dx, int dy, uint32_t amul = 256)
{
    int x0 = std::max(0, dx), x1 = std::min(dst.w, dx + sw);
    int y0 = std::max(0, dy), y1 = std::min(dst.h, dy + sh);
    for (int y = y0; y < y1; y++) {
        const uint32_t* s = src + (size_t)(y - dy) * sw + (x0 - dx);
        uint32_t* d = dst.p + (size_t)y * dst.w + x0;
        for (int x = x0; x < x1; x++, s++, d++) {
            uint32_t p = *s;
            if (!(p >> 24)) continue;
            if (amul != 256) p = MulPM(p, amul);
            *d = OverPM(p, *d);
        }
    }
}

// Bilinear blit with scale about the sprite point (scx, scy) placed at (dcx, dcy).
// tint scales rgb only (0..256), amul scales everything (0..256).
void BlitXform(View dst, const uint32_t* src, int sw, int sh, float scx, float scy, float dcx, float dcy, float sx, float sy,
               uint32_t tint, uint32_t amul)
{
    int x0 = std::max(0, (int)std::floor(dcx - scx * sx) - 1), x1 = std::min(dst.w, (int)std::ceil(dcx + (sw - scx) * sx) + 1);
    int y0 = std::max(0, (int)std::floor(dcy - scy * sy) - 1), y1 = std::min(dst.h, (int)std::ceil(dcy + (sh - scy) * sy) + 1);
    float isx = 1.f / sx, isy = 1.f / sy;
    auto fetch = [&](int x, int y) -> uint32_t {
        return ((unsigned)x < (unsigned)sw && (unsigned)y < (unsigned)sh) ? src[(size_t)y * sw + x] : 0u;
    };
    for (int y = y0; y < y1; y++) {
        float v = ((float)y + 0.5f - dcy) * isy + scy - 0.5f;
        int iy = (int)std::floor(v);
        uint32_t fy = (uint32_t)((v - (float)iy) * 256.f);
        uint32_t* d = dst.p + (size_t)y * dst.w;
        for (int x = x0; x < x1; x++) {
            float u = ((float)x + 0.5f - dcx) * isx + scx - 0.5f;
            int ix = (int)std::floor(u);
            uint32_t fx = (uint32_t)((u - (float)ix) * 256.f);
            uint32_t a = Lerp8(fetch(ix, iy), fetch(ix + 1, iy), fx);
            uint32_t b = Lerp8(fetch(ix, iy + 1), fetch(ix + 1, iy + 1), fx);
            uint32_t p = Lerp8(a, b, fy);
            if (!(p >> 24)) continue;
            if (tint != 256) p = TintPM(p, tint);
            if (amul != 256) p = MulPM(p, amul);
            d[x] = OverPM(p, d[x]);
        }
    }
}

// ---- baked layers ------------------------------------------------------------------------------
struct Layer {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    void Init(int ww, int hh) { w = ww; h = hh; px.assign((size_t)ww * hh, 0u); }
};

struct Cache {
    bool valid = false;
    float scale = 0.f;
    uint32_t titleW = 0, detailW = 0, iconW = 0, iconH = 0;
    uint64_t iconHash = 0; // = Content::revision
    bool hasIcon = false;

    int D = 0, R = 0, m = 0, Wp = 0, Wfr = 0, Hfr = 0, textLeft = 0, CW = 0;
    Layer pill;                                  // resting width, with shadow
    Layer base, lit, orb, icon, restOrb, restIcon; // CW x CW, centred
};

thread_local Cache gCache;
thread_local std::vector<uint32_t> gScratch, gScene, gCircle;

void BakePill(Cache& c, float s)
{
    const int W = c.Wp + 2 * c.m, H = c.D + 2 * c.m;
    c.pill.Init(W, H);
    const float R = (float)c.R, cy = (float)c.m + R;
    const float x0 = (float)c.m + R, x1 = (float)(c.m + c.Wp) - R;
    const float sigma = 3.6f * s, shOff = 1.f * s;
    for (int y = 0; y < H; y++) {
        float py = (float)y + 0.5f;
        float vt = Clamp01((py - (float)c.m) / (float)c.D);
        float body = Lerp(0.285f, 0.205f, vt);
        float rim = Lerp(0.50f, 0.40f, vt);
        const float outline = 0.03f;
        for (int x = 0; x < W; x++) {
            float px = (float)x + 0.5f;
            float cx = std::min(std::max(px, x0), x1);
            float d = std::hypot(px - cx, py - cy) - R;
            float ds = std::hypot(px - cx, py - shOff - cy) - R;
            float sa = 0.85f * 0.5f * std::erfc(ds / (sigma * 1.41421356f));
            float col = Lerp(outline, rim, Cov(d + 3.5f * s));
            col = Lerp(col, body, Cov(d + 5.f * s));
            float a = Cov(d);
            PM fill = FromStraight(col, col, col, a);
            PM sh = {0.f, 0.f, 0.f, sa};
            c.pill.px[(size_t)y * W + x] = Pack(Over(fill, sh));
        }
    }
}

// Silver sphere, our own shading: no logo.
PM OrbPixel(float rx, float ry, float r, float ro)
{
    float cov = Cov(r - ro);
    if (cov <= 0.f) return {0, 0, 0, 0};
    float nx = rx / ro, ny = ry / ro;
    float n2 = std::min(1.f, nx * nx + ny * ny);
    float nz = std::sqrt(1.f - n2);
    float lx = -0.40f, ly = -0.55f, lz = 0.73f;
    float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    float diff = std::max(0.f, (nx * lx + ny * ly + nz * lz) / ll);
    float rimDark = std::pow(nz, 0.55f);
    float v = (0.22f + 0.62f * diff) * (0.55f + 0.45f * rimDark);
    // soft specular blob up-left
    float hx = nx + 0.32f, hy = ny + 0.36f;
    float spec = std::exp(-(hx * hx + hy * hy) / 0.045f) * 0.55f;
    // faint bounce light from below
    float bounce = std::max(0.f, ny * 0.5f) * (1.f - nz) * 0.25f;
    v = Clamp01(v + spec + bounce);
    return FromStraight(v * 0.98f, v, v * 1.02f, cov);
}

PM IconPixel(const Cache&, const uint32_t* icon, uint32_t iw, uint32_t ih, float rx, float ry, float r, float ri)
{
    float cov = Cov(r - ri);
    if (cov <= 0.f) return {0, 0, 0, 0};
    float f = (float)iw / (2.f * ri); // texels per output pixel
    float u = (rx + ri) / (2.f * ri) * (float)iw;
    float v = (ry + ri) / (2.f * ri) * (float)ih;
    auto tex = [&](int x, int y) -> PM {
        x = std::min(std::max(x, 0), (int)iw - 1); y = std::min(std::max(y, 0), (int)ih - 1);
        uint32_t p = icon[(size_t)y * iw + x];
        float a = (float)(p >> 24) / 255.f;
        return {(float)(p & 255) / 255.f * a, (float)((p >> 8) & 255) / 255.f * a, (float)((p >> 16) & 255) / 255.f * a, a};
    };
    PM acc = {0, 0, 0, 0};
    if (f > 1.f) { // box filter over the footprint
        float hu0 = u - f * 0.5f, hu1 = u + f * 0.5f, hv0 = v - f * 0.5f, hv1 = v + f * 0.5f;
        float wsum = 0.f;
        for (int y = (int)std::floor(hv0); y < (int)std::ceil(hv1); y++)
            for (int x = (int)std::floor(hu0); x < (int)std::ceil(hu1); x++) {
                float wx = std::min((float)x + 1.f, hu1) - std::max((float)x, hu0);
                float wy = std::min((float)y + 1.f, hv1) - std::max((float)y, hv0);
                float w = wx * wy;
                if (w <= 0.f) continue;
                PM t = tex(x, y);
                acc.r += t.r * w; acc.g += t.g * w; acc.b += t.b * w; acc.a += t.a * w;
                wsum += w;
            }
        if (wsum > 0.f) { acc.r /= wsum; acc.g /= wsum; acc.b /= wsum; acc.a /= wsum; }
    } else { // bilinear
        float uu = u - 0.5f, vv = v - 0.5f;
        int ix = (int)std::floor(uu), iy = (int)std::floor(vv);
        float fx = uu - (float)ix, fy = vv - (float)iy;
        PM t00 = tex(ix, iy), t10 = tex(ix + 1, iy), t01 = tex(ix, iy + 1), t11 = tex(ix + 1, iy + 1);
        auto mix = [&](float a, float b, float cc, float dd) { return Lerp(Lerp(a, b, fx), Lerp(cc, dd, fx), fy); };
        acc = {mix(t00.r, t10.r, t01.r, t11.r), mix(t00.g, t10.g, t01.g, t11.g), mix(t00.b, t10.b, t01.b, t11.b), mix(t00.a, t10.a, t01.a, t11.a)};
    }
    return {acc.r * cov, acc.g * cov, acc.b * cov, acc.a * cov};
}

void BakeCircle(Cache& c, const Content& content, float s)
{
    const int CW = c.CW;
    c.base.Init(CW, CW); c.lit.Init(CW, CW); c.orb.Init(CW, CW); c.icon.Init(CW, CW);
    const float cc = (float)CW * 0.5f;
    const float rd = kDiskR * s, ro = kRingOuter * s, ri = kRingInner * s, gap = kGapHalf * s, rc = kCentreR * s;
    for (int y = 0; y < CW; y++) {
        for (int x = 0; x < CW; x++) {
            float rx = (float)x + 0.5f - cc, ry = (float)y + 0.5f - cc;
            float r = std::hypot(rx, ry);
            size_t i = (size_t)y * CW + x;

            // ring segments (4 arcs, cross-shaped gaps), top-left lit green
            float ringCov = std::min(Cov(r - ro), Cov(ri - r));
            float inGap = Cov(std::min(std::fabs(rx), std::fabs(ry)) - gap);
            float ringA = ringCov * (1.f - inGap);
            float diskA = Cov(r - rd);

            PM base = FromStraight(0.02f, 0.02f, 0.02f, diskA);
            if (ringA > 0.f) {
                PM seg;
                if (rx < 0.f && ry < 0.f) {
                    float k = Clamp01(0.5f + (-(rx + ry)) / (2.f * 1.41421356f * ro) * 1.0f);
                    seg = FromStraight(Lerp(0.35f, 0.43f, k), Lerp(0.72f, 0.78f, k), Lerp(0.09f, 0.13f, k), ringA);
                } else {
                    float g = 0.45f + 0.05f * Clamp01(0.5f - ry / (2.f * ro));
                    seg = FromStraight(g, g, g, ringA);
                }
                base = Over(seg, base);
            }
            c.base.px[i] = Pack(base);

            // lit ring + outward glow, faded in by the pulse
            PM lit = FromStraight(0.62f, 1.0f, 0.26f, ringA);
            if (r > rd - 2.f * s) {
                float ga = 0.75f * std::exp(-((r - rd) / (4.f * s)) * ((r - rd) / (4.f * s)));
                ga *= Clamp01((r - (rd - 2.f * s)) / (2.f * s)); // keep it off the ring interior
                PM glow = FromStraight(0.33f, 1.0f, 0.12f, ga);
                lit = Over(lit, glow);
            }
            c.lit.px[i] = Pack(lit);

            c.orb.px[i] = Pack(OrbPixel(rx, ry, r, rc));
        }
    }
    if (content.icon && content.iconWidth && content.iconHeight) {
        const float rci = rc + 0.5f * s;
        for (int y = 0; y < CW; y++)
            for (int x = 0; x < CW; x++) {
                float rx = (float)x + 0.5f - cc, ry = (float)y + 0.5f - cc;
                c.icon.px[(size_t)y * CW + x] = Pack(IconPixel(c, content.icon, content.iconWidth, content.iconHeight, rx, ry, std::hypot(rx, ry), rci));
            }
    } else {
        c.icon.px = c.orb.px;
    }
    c.restOrb = c.base;
    c.restIcon = c.base;
    for (size_t i = 0; i < c.base.px.size(); i++) {
        c.restOrb.px[i] = OverPM(c.orb.px[i], c.base.px[i]);
        c.restIcon.px[i] = OverPM(c.icon.px[i], c.base.px[i]);
    }
}

void EnsureCache(const Content& content, float scale)
{
    Cache& c = gCache;
    bool hasIcon = content.icon && content.iconWidth && content.iconHeight;
    uint32_t tw = content.title ? content.titleWidth : 0, dw = content.detail ? content.detailWidth : 0;
    if (c.valid && c.scale == scale && c.iconHash == content.revision && c.titleW == tw && c.detailW == dw && c.hasIcon == hasIcon &&
        c.iconW == content.iconWidth && c.iconH == content.iconHeight)
        return;

    c.valid = true;
    c.scale = scale; c.titleW = tw; c.detailW = dw; c.hasIcon = hasIcon; c.iconHash = content.revision; // iconHash holds the revision
    c.iconW = content.iconWidth; c.iconH = content.iconHeight;

    c.R = std::max(2, (int)std::lround(kPillH * scale * 0.5f));
    c.D = 2 * c.R;
    c.m = (int)std::ceil(kMargin * scale);
    c.textLeft = (int)std::lround(kTextLeft * scale);
    c.Wp = std::max(2 * c.D, c.textLeft + (int)std::max(tw, dw) + (int)std::lround(kPadRight * scale));
    c.Wfr = (int)std::ceil((double)c.Wp * (kOvershoot + 0.005)) + 2 * c.m;
    c.Hfr = c.D + 2 * c.m;
    c.CW = 2 * (int)std::ceil((float)c.R + kHalo * scale);
    BakePill(c, scale);
    BakeCircle(c, content, scale);
}

// Text image blit (straight-alpha source) into the premultiplied scratch, clipped to x < clipX with a soft edge.
void BlitText(View dst, const uint32_t* img, int iw, int ih, int dx, int dy, int clipX, int soft)
{
    if (!img || iw <= 0 || ih <= 0) return;
    int xEnd = std::min({iw, dst.w - dx, clipX - dx});
    int xs = std::max(0, -dx);
    for (int y = std::max(0, -dy); y < ih && y + dy < dst.h; y++) {
        const uint32_t* s = img + (size_t)y * iw;
        uint32_t* d = dst.p + (size_t)(y + dy) * dst.w + dx;
        for (int x = xs; x < xEnd; x++) {
            uint32_t p = s[x];
            uint32_t a = p >> 24;
            if (!a) continue;
            int toClip = clipX - (dx + x); // pixels to the clip edge
            if (toClip < soft) a = a * (uint32_t)toClip / (uint32_t)soft;
            if (!a) continue;
            uint32_t r = ((p & 255) * a + 127) / 255, g = (((p >> 8) & 255) * a + 127) / 255, b = (((p >> 16) & 255) * a + 127) / 255;
            d[x] = OverPM(r | (g << 8) | (b << 16) | (a << 24), d[x]);
        }
    }
}

const uint32_t* RecipTable()
{
    static uint32_t tab[256];
    static bool init = [] {
        tab[0] = 0;
        for (uint32_t a = 1; a < 256; a++) tab[a] = (255u * 65536u + a / 2) / a;
        return true;
    }();
    (void)init;
    return tab;
}
} // namespace

double Duration() { return kDuration; }

Frame Render(const Content& content, double t, float scale)
{
    Frame f;
    if (!(scale > 0.f)) return f;
    EnsureCache(content, scale);
    Cache& c = gCache;
    f.width = (uint32_t)c.Wfr;
    f.height = (uint32_t)c.Hfr;
    f.offsetX = -(c.Wp / 2) - c.m;
    f.offsetY = -c.D - c.m;
    if (!(t >= 0.0) || t >= kDuration) return f;
    f.visible = true;

    const int W = c.Wfr, H = c.Hfr, m = c.m, R = c.R, D = c.D, L = m + R;

    // ---- timeline ----
    double w; // pill outer width (pixels), grows to the right from the circle
    double fade = 1.0;
    float popScale = 1.f;
    if (t < kPopEnd) {
        double p = t / kPopEnd;
        popScale = 1.15f - 0.15f * (float)EaseOutCubic(p);
        fade = EaseOutCubic(p);
        w = D;
    } else if (t < kGrowEnd) {
        w = D + ((double)c.Wp * kOvershoot - D) * EaseOutQuad((t - kPopEnd) / (kGrowEnd - kPopEnd));
    } else if (t < kSettleEnd) {
        w = (double)c.Wp * (kOvershoot - (kOvershoot - 1.0) * Smooth((t - kGrowEnd) / (kSettleEnd - kGrowEnd)));
    } else if (t < kCollapseStart) {
        w = c.Wp;
    } else {
        double u = Clamp01((t - kCollapseStart) / (kDuration - kCollapseStart));
        w = c.Wp + (double)(D - c.Wp) * Smooth(u);
        fade = 1.0 - Smooth(u);
    }
    int wi = std::max(D, (int)std::lround(w));

    // circle centre content state
    int state = 0;      // 0 orb, 1 icon
    float squash = 1.f; // horizontal scale of the centre
    float dark = 1.f;
    bool flipping = false;
    if (c.hasIcon) {
        for (double fc : kFlipCentres) {
            if (t >= fc + kFlipHalf) { state ^= 1; continue; }
            if (t >= fc - kFlipHalf) {
                double p = (t - (fc - kFlipHalf)) / (2 * kFlipHalf);
                float cv = (float)std::fabs(std::cos(kPi * p));
                if (p >= 0.5) state ^= 1;
                squash = std::max(cv, 0.03f);
                dark = 1.f - 0.75f * (1.f - cv);
                flipping = true;
            }
            break;
        }
    }
    float pulse = 0.f;
    if (t >= kPulseStart && t < kPulseEnd) pulse = (float)std::sin(kPi * (t - kPulseStart) / (kPulseEnd - kPulseStart));

    // ---- compose (premultiplied) into the scratch buffer ----
    gScratch.assign((size_t)W * H, 0u);
    View S{gScratch.data(), W, H};
    const int total = wi + 2 * m;
    {
        const uint32_t* sp = c.pill.px.data();
        const int sw = c.pill.w;
        for (int y = 0; y < H; y++) {
            const uint32_t* row = sp + (size_t)y * sw;
            uint32_t* d = S.p + (size_t)y * W;
            std::memcpy(d, row, (size_t)L * 4);
            std::fill(d + L, d + total - L, row[L]);
            std::memcpy(d + total - L, row + (sw - L), (size_t)L * 4);
        }
    }

    // text, revealed by the right edge
    {
        int clipX = m + wi - (int)std::lround(0.35 * R);
        int soft = std::max(1, (int)std::lround(4.f * scale));
        int tx = m + c.textLeft;
        int cy = m + R;
        if (content.title)
            BlitText(S, content.title, (int)content.titleWidth, (int)content.titleHeight, tx,
                     cy + (int)std::lround(kTitleDy * scale) - (int)content.titleHeight / 2, clipX, soft);
        if (content.detail)
            BlitText(S, content.detail, (int)content.detailWidth, (int)content.detailHeight, tx,
                     cy + (int)std::lround(kDetailDy * scale) - (int)content.detailHeight / 2, clipX, soft);
    }

    // circle
    {
        const int CW = c.CW;
        const int ox = L - CW / 2, oy = m + R - CW / 2;
        if (!flipping && pulse <= 0.f) {
            const Layer& rest = state ? c.restIcon : c.restOrb;
            BlitOver(S, rest.px.data(), CW, CW, ox, oy);
        } else {
            gCircle.assign((size_t)CW * CW, 0u);
            View C{gCircle.data(), CW, CW};
            BlitOver(C, c.base.px.data(), CW, CW, 0, 0);
            const Layer& centre = state ? c.icon : c.orb;
            if (flipping)
                BlitXform(C, centre.px.data(), CW, CW, CW * 0.5f, CW * 0.5f, CW * 0.5f, CW * 0.5f, squash, 1.f, (uint32_t)(dark * 256.f), 256);
            else
                BlitOver(C, centre.px.data(), CW, CW, 0, 0);
            if (pulse > 0.f) BlitOver(C, c.lit.px.data(), CW, CW, 0, 0, (uint32_t)(pulse * 256.f));
            float ps = 1.f + 0.08f * pulse;
            if (ps != 1.f)
                BlitXform(S, gCircle.data(), CW, CW, CW * 0.5f, CW * 0.5f, (float)L, (float)(m + R), ps, ps, 256, 256);
            else
                BlitOver(S, gCircle.data(), CW, CW, ox, oy);
        }
    }

    int cols = std::min(W, total + (int)(0.2 * L) + 1);
    if (popScale != 1.f) { // the whole (circle-sized) toast pops
        int pc = std::min(W, 2 * L + 2);
        gScene.assign((size_t)pc * H, 0u);
        for (int y = 0; y < H; y++) std::memcpy(&gScene[(size_t)y * pc], S.p + (size_t)y * W, (size_t)pc * 4);
        gScratch.assign((size_t)W * H, 0u);
        S.p = gScratch.data();
        BlitXform(S, gScene.data(), pc, H, (float)L, (float)(m + R), (float)L, (float)(m + R), popScale, popScale, 256, 256);
        cols = std::min(W, 2 * L + (int)(0.2 * L) + 2);
    }

    // ---- fade + convert to straight alpha ----
    uint32_t fm = (uint32_t)(Clamp01(fade) * 256.0);
    const uint32_t* recip = RecipTable();
    f.pixels.assign((size_t)W * H, 0u);
    for (int y = 0; y < H; y++) {
        const uint32_t* s = S.p + (size_t)y * W;
        uint32_t* o = f.pixels.data() + (size_t)y * W;
        for (int x = 0; x < cols; x++) {
            uint32_t p = s[x];
            if (fm < 256) p = MulPM(p, fm);
            uint32_t a = p >> 24;
            if (a == 0) continue;
            if (a == 255) { o[x] = p; continue; }
            uint32_t k = recip[a];
            uint32_t r = std::min(255u, ((p & 255) * k + 0x8000u) >> 16);
            uint32_t g = std::min(255u, (((p >> 8) & 255) * k + 0x8000u) >> 16);
            uint32_t b = std::min(255u, (((p >> 16) & 255) * k + 0x8000u) >> 16);
            o[x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }
    return f;
}

std::vector<float> SynthesizeChime(uint32_t sampleRate)
{
    const double total = 0.34;
    const size_t n = (size_t)(total * sampleRate);
    std::vector<float> out(n, 0.f);
    const double sr = (double)sampleRate;

    // part 1: soft downward blip at 0
    {
        double phase = 0.0;
        for (size_t i = 0; i < (size_t)(0.05 * sr) && i < n; i++) {
            double tt = (double)i / sr;
            double freq = 560.0 + 380.0 * std::exp(-tt / 0.012);
            phase += 2.0 * kPi * freq / sr;
            double env = (1.0 - std::exp(-tt / 0.0008)) * std::exp(-tt / 0.010);
            out[i] += (float)(0.30 * env * std::sin(phase));
        }
    }
    // part 2: bright bell 0.1 s later - three partials plus a soft body tone, ~0.2 s decay
    {
        struct Partial { double ratio, amp, tau; };
        const Partial parts[] = {{1.0, 0.50, 0.060}, {2.0, 0.30, 0.045}, {2.76, 0.14, 0.030}, {0.5, 0.12, 0.070}};
        const double f0 = 1568.0, start = 0.10;
        for (size_t i = (size_t)(start * sr); i < n; i++) {
            double tt = (double)i / sr - start;
            double att = 1.0 - std::exp(-tt / 0.0025);
            double v = 0.0;
            for (const Partial& p : parts)
                v += p.amp * std::exp(-tt / p.tau) * std::sin(2.0 * kPi * f0 * p.ratio * tt + (p.ratio == 2.76 ? 0.7 : 0.0));
            out[i] += (float)(att * v);
        }
    }
    // smooth end (last 15 ms), normalise to a 0.5 peak
    size_t fadeN = (size_t)(0.015 * sr);
    for (size_t i = 0; i < fadeN && i < n; i++) {
        double k = 0.5 * (1.0 + std::cos(kPi * (double)i / (double)fadeN)); // 1 -> 0
        out[n - 1 - i] *= (float)(1.0 - k);
    }
    float peak = 0.f;
    for (float v : out) peak = std::max(peak, std::fabs(v));
    if (peak > 0.f)
        for (float& v : out) v *= 0.5f / peak;
    return out;
}
}
