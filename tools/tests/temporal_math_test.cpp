#include <gpu/temporal_math.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace gpu::temporal;
static int checks = 0;
static void Require(bool ok, const char* message)
{
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Near(double a, double b, const char* message, double tolerance = 1e-8)
{
    Require(std::isfinite(a) && std::abs(a - b) <= tolerance, message);
}
static Matrix Projection(double farPlane = 100)
{
    // Independent analytic pinhole: near=1, horizontal/vertical slopes=1/2.
    return {1,0,0,0, 0,2,0,0, 0,0,farPlane/(farPlane-1),1, 0,0,-farPlane/(farPlane-1),0};
}
static Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix c{};
    for (int i=0;i<4;++i) for(int j=0;j<4;++j) for(int k=0;k<4;++k) c[4*i+j]+=a[4*i+k]*b[4*k+j];
    return c;
}
static Camera Make(const Matrix& m, const Viewport& v)
{
    auto c = Camera::Create(m,v); Require(c.has_value(), "valid camera"); return *c;
}
// Oracle uses scalar pinhole equations, never production Transform/Inverse/Reproject.
static Sample Analytic(double x, double y, double z, const Viewport& v)
{
    return {v.x + v.width*.5*(1+x/z+v.halfPixelNdcX),
            v.y + v.height*.5*(1-v.ndcYSign*2*y/z-v.halfPixelNdcY),
            (100/z-1)/99};
}
int main()
{
    const Viewport raster{0,0,1280,720,1,1./1280,-1./720};
    const auto camera = Make(Projection(),raster);
    // Independent known world points, rather than a round trip through the same projection code.
    for (const Vector point : {Vector{0,0,10,1}, Vector{1,2,10,1}, Vector{-3,-1,20,1}})
    {
        const auto sample = Analytic(point[0],point[1],point[2],raster);
        const auto r = Reproject(sample,camera,camera);
        Require(bool(r), "known point accepted");
        for (int i=0;i<4;++i) Near(r.world[i],point[i],"known world reconstructed");
        Near(r.previous.x,sample.x,"static x"); Near(r.previous.y,sample.y,"static y");
        Near(r.previous.depth,sample.depth,"static depth");
    }
    Near(Analytic(0,0,10,raster).x,640.5,"half pixel center x");
    Near(Analytic(0,0,10,raster).y,360.5,"half pixel center y");

    // Previous camera located +1 world X: a static world point moves left by 640/z.
    Matrix translation{1,0,0,0, 0,1,0,0, 0,0,1,0, -1,0,0,1};
    auto previous = Make(Multiply(translation,Projection()),raster);
    auto currentSample = Analytic(1,2,10,raster);
    auto shifted = Reproject(currentSample,camera,previous);
    Require(bool(shifted),"translation accepted");
    Near(shifted.previous.x,currentSample.x-64,"translation analytic x displacement");
    Near(shifted.previous.y,currentSample.y,"translation y unchanged");

    // Explicit world-to-camera yaw: x'=cos(t)x-sin(t)z, z'=sin(t)x+cos(t)z.
    const double angle=.1, c=std::cos(angle), s=std::sin(angle);
    Matrix yaw{c,0,s,0, 0,1,0,0, -s,0,c,0, 0,0,0,1};
    previous=Make(Multiply(yaw,Projection()),raster);
    auto rotated=Reproject(currentSample,camera,previous);
    const auto oracle=Analytic(c-10*s,2,s+10*c,raster);
    Require(bool(rotated),"rotation accepted");
    Near(rotated.previous.x,oracle.x,"rotation analytic x");
    Near(rotated.previous.y,oracle.y,"rotation analytic y");
    Near(rotated.previous.depth,oracle.depth,"rotation analytic depth");

    const Viewport resized{31,17,1920,1080,-1,0,0};
    auto resize=Reproject(currentSample,camera,Make(Projection(),resized));
    const auto resizeOracle=Analytic(1,2,10,resized);
    Require(bool(resize),"offset viewport, resize and Y flip accepted");
    Near(resize.previous.x,resizeOracle.x,"resized x"); Near(resize.previous.y,resizeOracle.y,"resized y");
    auto back=Reproject(resizeOracle,Make(Projection(),resized),camera);
    Require(bool(back),"inverse flipped viewport"); Near(back.previous.y,currentSample.y,"flipped input inverse");

    Require(!Camera::Create(Matrix{},raster),"singular matrix rejected");
    auto bad=Projection(); bad[0]=std::numeric_limits<double>::infinity();
    Require(!Camera::Create(bad,raster),"nonfinite matrix rejected");
    bad=Projection(); bad[0]=1e-15;
    Require(!Camera::Create(bad,raster),"ill-conditioned matrix rejected");
    for (Viewport v : {Viewport{0,0,0,720},Viewport{0,0,1280,720,0},Viewport{0,0,1280,720,1,std::numeric_limits<double>::quiet_NaN(),0}})
        Require(!Camera::Create(Projection(),v),"invalid viewport rejected");
    for (double depth : {0.,-0.1,1.1,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        Require(Reproject({640.5,360.5,depth},camera,camera).rejection==Rejection::InvalidSample,"invalid or clear depth rejected");
    Require(Reproject({0,360.5,.1},camera,camera).rejection==Rejection::CurrentOutside,"integer edge is not a center");
    Require(Reproject({1280,360.5,.1},camera,camera).rejection==Rejection::CurrentOutside,"current outside right");
    Require(Reproject({640.5,std::numeric_limits<double>::infinity(),.1},camera,camera).rejection==Rejection::InvalidSample,"nonfinite pixel");
    Require(InsideCenters(.5,.5,raster)&&InsideCenters(1279.5,719.5,raster),"edge centers allowed");

    translation[12]=100;
    Require(Reproject(currentSample,camera,Make(Multiply(translation,Projection()),raster)).rejection==Rejection::PreviousOutside,"history outside rejected");
    translation[12]=0; translation[14]=-10;
    Require(Reproject(Analytic(0,0,10,raster),camera,Make(Multiply(translation,Projection()),raster)).rejection==Rejection::InvalidPreviousW,"zero previous clip W");
    translation[14]=-11;
    Require(Reproject(currentSample,camera,Make(Multiply(translation,Projection()),raster)).rejection==Rejection::InvalidPreviousW,"behind previous camera");
    translation[14]=-9.5;
    Require(Reproject(currentSample,camera,Make(Multiply(translation,Projection()),raster)).rejection==Rejection::PreviousDepth,"before previous near plane");
    translation[14]=100;
    Require(Reproject(currentSample,camera,Make(Multiply(translation,Projection()),raster)).rejection==Rejection::PreviousDepth,"beyond previous far plane");
    const auto shortCamera=Make(Projection(2),raster);
    auto near=Reproject({640.5,360.5,1},shortCamera,shortCamera);
    Require(bool(near),"exact near plane accepted"); Near(near.world[2],1,"near-plane world Z");
    Matrix negative=Projection(); for (double& x:negative) x=-x;
    Require(Reproject(currentSample,Make(negative,raster),camera).rejection==Rejection::InvalidWorldW,"negative reconstructed W");
    Matrix infinite=Projection(); infinite[10]=.5; infinite[14]=-1;
    Require(Reproject({640.5,360.5,.5},Make(infinite,raster),camera).rejection==Rejection::InvalidWorldW,"world at infinity");
    std::printf("PASS: %d temporal camera math checks (CPU only; no runtime temporal contract)\n",checks);
}
