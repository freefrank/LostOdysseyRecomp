// Exact-bit image oracle and command/rectangle counts for partial clear coverage.
#include "gpu/pending_clear.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

using namespace gpu::pending_clear;
namespace {
unsigned checks = 0;
void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); }
}
Op Color(float value, std::vector<Rect> rects) {
    const float rgba[4] = {value,-value,0.5f,1};
    return ColorOp(rgba,std::move(rects));
}
struct Pixel {
    std::array<uint32_t,4> color{};
    uint32_t depth=0, stencil=0;
    bool operator==(const Pixel&) const = default;
};
struct Image {
    static constexpr uint32_t width=16, height=12;
    std::array<Pixel,width*height> pixels{};
    uint64_t calls=0, rectangles=0, aspectPixels=0;
    void Apply(const Op& op) {
        ++calls;
        const auto fill = [&](const Rect& r) {
            ++rectangles;
            Check(r.left>=0 && r.top>=0 && r.right<=int32_t(width) && r.bottom<=int32_t(height),"oracle rectangle in bounds");
            for(int y=r.top;y<r.bottom;++y) for(int x=r.left;x<r.right;++x) {
                auto& p=pixels[y*width+x];
                if(op.aspects&gpu::pending_clear::Color) {
                    for(unsigned c=0;c<4;++c) p.color[c]=std::bit_cast<uint32_t>(op.color[c]);
                    ++aspectPixels;
                }
                if(op.aspects&Depth) { p.depth=std::bit_cast<uint32_t>(op.depth); ++aspectPixels; }
                if(op.aspects&Stencil) { p.stencil=op.stencil; ++aspectPixels; }
            }
        };
        if(op.Whole()) fill({0,0,int32_t(width),int32_t(height)});
        else for(const auto& r:op.rects) fill(r);
    }
    void Flush(Queue& q) {
        auto ops=q.Take();
        for(const auto& op:ops) Apply(op);
        q.Recycle(std::move(ops));
    }
};
void Rules() {
    Queue q;
    q.Record(Color(.25f,{{1,1,5,5}}),16,12);
    q.Record(Color(.75f,{{0,0,6,6}}),16,12);
    Check(q.Size()==1 && q.Ops()[0].color[0]==.75f,"new partial clear eliminates an overwritten old clear");
    q.Drop();
    q.Record(Color(.25f,{{0,0,8,8}}),16,12);
    for(unsigned i=0;i<4000;++i) q.Record(Color(.25f,{{1,1,5,5}}),16,12);
    Check(q.Size()==1 && q.Ops()[0].rects.size()==1,"contained equal-value clears do not accumulate rectangles");
    q.Record(Color(.25f,{{7,7,9,9}}),16,12);
    Check(q.Ops()[0].rects.size()==2,"partial overlap is not treated as containment");
    q.Record(Color(.25f,{{6,6,10,10}}),16,12);
    Check(q.Ops()[0].rects.size()==2 && q.Ops()[0].rects.back().left==6,"equal-value merging replaces contained rectangles");

    Queue d;
    d.Record(DepthOp(true,true,.25f,7,{{1,1,3,3}}),16,12);
    d.Record(DepthOp(true,false,.5f,0,{{0,0,4,4}}),16,12);
    Check(d.Size()==2 && d.Ops()[0].aspects==Stencil,"partial depth overwrite preserves old stencil");
    d.Record(DepthOp(false,true,0,9,{{0,0,5,5}}),16,12);
    Check(d.Size()==2 && d.Ops()[0].aspects==Depth && d.Ops()[1].stencil==9,"partial stencil overwrite preserves depth");

    Queue whole;
    whole.Record(Color(.25f,{}),16,12);
    whole.Record(Color(.5f,{{0,0,4,4}}),16,12);
    Check(whole.Size()==2 && whole.Ops()[0].Whole(),"partial overwrite cannot remove a whole clear");
    Queue multiple;
    multiple.Record(Color(.25f,{{1,1,3,3},{3,3,4,4}}),16,12);
    multiple.Record(Color(.5f,{{0,0,5,5}}),16,12);
    Check(multiple.Size()==1,"single covering rectangle removes a wholly covered multi-rectangle clear");
    Queue gap;
    gap.Record(Color(.25f,{{0,0,10,10}}),16,12);
    gap.Record(Color(.5f,{{0,0,3,10},{7,0,10,10}}),16,12);
    Check(gap.Size()==2,"bounding box of disjoint incoming rectangles cannot erase the gap");

    Queue bounded;
    std::vector<Rect> rects(33,Rect{0,0,2,2});
    bounded.Record(Color(.25f,rects),16,12);
    bounded.Record(Color(.25f,{{0,0,2,2}}),16,12);
    Check(bounded.Ops()[0].rects.size()==34,"large rectangle lists keep the bounded fallback");
    Queue bits;
    bits.Record(DepthOp(true,false,0.0f,0,{{0,0,3,3}}),16,12);
    bits.Record(DepthOp(true,false,-0.0f,0,{{0,0,4,4}}),16,12);
    Check(bits.Size()==1 && std::bit_cast<uint32_t>(bits.Ops()[0].depth)==0x80000000u,"overwrite retains exact new signed-zero bits");

    Image eager,lazy;
    Queue repeated;
    for(unsigned i=0;i<4000;++i) {
        const auto op=Color(.25f,{{1,1,5,5}});
        eager.Apply(op); repeated.Record(op,16,12);
    }
    lazy.Flush(repeated);
    Check(eager.pixels==lazy.pixels && eager.rectangles==4000 && lazy.rectangles==1,"4000 repeated partial clears replay once with identical pixels");
    std::printf("PASS: repeated-clear model: rectangles %llu -> %llu, aspect-pixel writes %llu -> %llu\n",
        (unsigned long long)eager.rectangles,(unsigned long long)lazy.rectangles,
        (unsigned long long)eager.aspectPixels,(unsigned long long)lazy.aspectPixels);
}
void Randomized() {
    uint64_t eagerRects=0,lazyRects=0,eagerPixels=0,lazyPixels=0;
    constexpr std::array<uint32_t,8> values{0,0x80000000u,0x3f000000u,0x3f800000u,0x7fc01234u,0x7fc05678u,0x00000001u,0x3e800000u};
    for(unsigned seed=1;seed<=10000;++seed) {
        std::mt19937 rng(seed);
        Queue q;
        Image eager,lazy;
        const bool depth=seed&1;
        for(unsigned step=0;step<50;++step) {
            Op op;
            if(depth) op=DepthOp(rng()%2,rng()%2,std::bit_cast<float>(values[rng()%values.size()]),rng());
            else {
                float color[4];for(auto& c:color)c=std::bit_cast<float>(values[rng()%values.size()]);
                op=ColorOp(color);
            }
            if(!op.aspects) continue;
            if(rng()%6) {
                const unsigned count=1+rng()%4;
                for(unsigned i=0;i<count;++i) {
                    const int x=rng()%16,y=rng()%12;
                    op.rects.push_back({x,y,x+1+int(rng()%(16-x)),y+1+int(rng()%(12-y))});
                }
            }
            // Match QueueClear's execution boundary before a full queue.
            if(q.Size()>=Queue::kMaxOps) lazy.Flush(q);
            eager.Apply(op);q.Record(op,16,12);
            if(rng()%7==0) {
                lazy.Flush(q);
                Check(eager.pixels==lazy.pixels,"all color/depth/stencil bits match at each read boundary");
            }
        }
        lazy.Flush(q);
        Check(eager.pixels==lazy.pixels,"all final color/depth/stencil bits match");
        Check(lazy.aspectPixels<=eager.aspectPixels,"coverage optimization never adds pixel writes");
        eagerRects+=eager.rectangles;lazyRects+=lazy.rectangles;
        eagerPixels+=eager.aspectPixels;lazyPixels+=lazy.aspectPixels;
    }
    std::printf("PASS: 10000 exact-bit sequences, rectangles %llu -> %llu, aspect-pixel writes %llu -> %llu\n",
        (unsigned long long)eagerRects,(unsigned long long)lazyRects,
        (unsigned long long)eagerPixels,(unsigned long long)lazyPixels);
}
}
int main(int argc, char** argv) {
    if(argc==1) Rules();
    else if(argc!=2 || std::strcmp(argv[1],"--random-only")!=0) return 2;
    Randomized();std::printf("PASS: %u checks\n",checks);
}
