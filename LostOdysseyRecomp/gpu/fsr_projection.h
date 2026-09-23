#pragma once
#include "temporal_math.h"
#include <optional>

namespace gpu::fsr {
struct Projection {
    float nearDistance = 0, verticalFovRadians = 0, depthScale = 0, depthBias = 0;
    double pole = 0, aspect = 0;
};

// Row-vector world*VP; remove the view rotation without inventing near/far.
inline std::optional<Projection> DeriveProjection(const temporal::Matrix& m, double expectedAspect)
{
    for (double v : m) if (!std::isfinite(v)) return {};
    if (!(expectedAspect > 0) || !std::isfinite(expectedAspect)) return {};
    using V = std::array<double, 3>;
    const V x{m[0],m[4],m[8]}, y{m[1],m[5],m[9]}, z{m[2],m[6],m[10]}, w{m[3],m[7],m[11]};
    const auto dot=[](const V& a,const V& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const double s2=dot(w,w), s=std::sqrt(s2);
    if (!(s > 1e-8)) return {}; // Orthographic/non-projective matrix.
    const double a=dot(z,w)/s2, q=-(m[14]-a*m[15])/s, pole=1-a;
    const double ox=dot(x,w)/s2, oy=dot(y,w)/s2;
    V right{},up{};
    for (int i=0;i<3;++i) {
        if (std::abs(z[i]-a*w[i]) > 1e-4*s) return {}; // Oblique depth.
        right[i]=x[i]/s-ox*w[i]/s; up[i]=y[i]/s-oy*w[i]/s;
    }
    const double fx=std::sqrt(dot(right,right)),fy=std::sqrt(dot(up,up));
    if (!(q>0) || !(a>0) || !(fx>0) || !(fy>0) ||
        std::abs(ox)>1e-3 || std::abs(oy)>1e-3 ||
        std::abs(dot(right,up))>1e-4*fx*fy ||
        std::abs(fy/fx-expectedAspect)>1e-3*expectedAspect) return {};
    const double nearDistance=q/a, fov=2*std::atan(1/fy);
    if (!std::isfinite(nearDistance) || nearDistance<=0 || !std::isfinite(fov) || fov<=0 || fov>=3.141592653589793) return {};
    return Projection{float(nearDistance),float(fov),float(1/a),float(-pole/a),pole,fy/fx};
}
}
