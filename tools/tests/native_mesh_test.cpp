#include "gpu/native_mesh.h"
#include "gpu/draw_state_cache.h"
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>
using namespace gpu::native_frontend;
using namespace gpu::renderer;
static void Check(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
static uint32_t BitsAt(const DrawState& state, size_t offset)
{
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(&state) + offset, 4);
    return value;
}
int main()
{
    Check(IndexPhysicalAddress(0xa1234562) == 0x01234562, "16-bit index subrange keeps bit one");
    Check(IndexPhysicalAddress(0xe1234562) == 0x01235562, "SDK E-region physical alias correction");
    const auto indexed16 = IndexedDraw(4, 231, 1, 0x40000000, 0xa0010020);
    Check(indexed16.initiator == 0x00e70004 && indexed16.dmaBase == 0x10022 &&
        indexed16.dmaSize == 0x800000e7, "16-bit SDK index count/start/endian contract");
    const auto indexed32 = IndexedDraw(6, 65535, 3, 0xe0000000, 0xe0010000);
    Check(indexed32.initiator == 0xffff0806 && indexed32.dmaBase == 0x1100c &&
        indexed32.dmaSize == 0xc001fffe, "32-bit SDK index count/start/endian contract");
    Check(QualifyDirty({0,0,1ull<<17}) == Reject::ShaderPrepare, "shader preparation has guest side effects");
    Check(QualifyDirty({0,0,1,0,0,1}) == Reject::DerivedPrepare, "derived state intersection cannot be cleared");
    Check(QualifyDirty({0,0,0,0,1ull<<49}) == Reject::StreamPrepare, "stream event preparation remains ordered legacy");
    DirtyState dirty{(1ull<<63)|1, (1ull<<62)|1,
        (1ull<<11)|(1ull<<12)|(1ull<<57)|(1ull<<21),
        (1ull<<31)|(1ull<<34), (1ull<<37)|(1ull<<38)|(1ull<<56), 0};
    PreparedMesh prepared;
    Check(Prepare(indexed16, dirty, prepared), "ordinary mesh with state changes accepted");
    std::array<uint32_t,kMaxWords> storage{};
    auto source = [](uint32_t offset) { return 0xb0000000u | offset; };
    Check(Encode(prepared, 37, source, storage), "encode complete owned delta");
    DecodedMesh decoded;
    Check(Decode(std::span(storage.data()+1, prepared.words-1), decoded), "validate native mesh");
    Check(decoded.producerRevision == 37, "producer revision transported by value");
    std::vector<uint32_t> registers(0x5003, 0x11111111);
    ApplyDeltas(decoded, [&](uint32_t index, uint32_t value){ registers[index] = value; });
    Check(registers[0x4000] == source(1920) && registers[0x43ff] == source(1920+4092), "VS dirty chunk ordering");
    Check(registers[0x4410] == source(6016+64) && registers[0x47ff] == source(6016+4092), "PS dirty chunk ordering");
    Check(registers[0x2200] == source(10548) && registers[0x2184] == source(10528+16), "main dirty bit rotation");
    Check(registers[0x2000] == source(10368) && registers[0x2114] == source(10444+80), "target/raster dirty bit rotation");
    Check(registers[0x4800] == source(1152) && registers[0x2280+20] == source(10596+80), "fetch and point state group ordering");
    Check(registers[0x2300] == source(10680) && registers[0x2387] == source(10832+28), "draw/polygon group rotation");
    Check(registers[0x4900] == source(10112) && registers[0x4927] == source(10112+156), "BOOL dirty bit flushes all 40 bool/loop words");
    Check(registers[0x5000] == 0 && registers[0x5001] == 0x25000 && registers[0x5002] == 0, "no-rollover fetch tail compatibility");
    Check(registers[0x2102] == 0 && registers[0x2101] == 0x11111111, "explicit index base and inherited caller state");
    // A replay observes new inherited values, while captured values cannot be
    // changed by producer overwrites of the original device state.
    registers[0x2101] = 99; registers[0x4000] = 42;
    ApplyDeltas(decoded, [&](uint32_t index, uint32_t value){ registers[index] = value; });
    Check(registers[0x2101] == 99 && registers[0x4000] == source(1920), "replay stable payload and inherited state");
    auto bad = storage;
    bad[2] |= 0x8000;
    Check(!Decode(std::span(bad.data()+1, prepared.words-1), decoded), "unknown state group rejected");
    bad = storage; bad[1]++;
    Check(!Decode(std::span(bad.data()+1, prepared.words-1), decoded), "length mismatch rejected before mutation");
    Check(!Decode(std::span(storage.data()+1, prepared.words-2), decoded), "truncated body rejected");
    bad = storage; bad[3] |= 0x4000;
    Check(!Decode(std::span(bad.data()+1, prepared.words-1), decoded), "unsupported initiator bits rejected");
    // All dirty masks fit a bounded record and decode without reading slack.
    std::mt19937_64 random(47);
    for (int trial = 0; trial < 300; ++trial) {
        DirtyState sample{random(),random(),random(),random(),random(),0};
        sample.main &= ~(15ull<<17); sample.misc &= ~(63ull<<49);
        Check(Prepare(AutoDraw(4,17,311),sample,prepared), "bounded random group preparation");
        Check(Encode(prepared,uint64_t(trial),source,storage), "bounded random group encoding");
        Check(Decode(std::span(storage.data()+1,prepared.words-1),decoded), "bounded random group decoding");
        Check(!Decode(std::span(storage.data()+1,prepared.words-2),decoded), "every truncated random record rejected");
    }
    // Explicit effective state follows legacy -> native -> legacy writes. It
    // does not reread cached nonzero scalars or freeze a zero-MMIO fallback.
    std::vector<uint8_t> mirror(registers.size()*4);
    auto putMirror = [&](uint32_t index,uint32_t value) {
        const auto guest = gpu::native_command::GuestWord(value);
        std::memcpy(mirror.data()+index*4,&guest,4);
    };
    registers.assign(registers.size(),0);
    auto bank = DrawWords::Legacy(registers,mirror.data());
    ExecutionDrawState cache;
    putMirror(0x2205,17);
    auto state = cache.ForDraw({},bank,{},{});
    Check(state.pipeline.modeCull == 17, "seed reads current zero fallback");
    putMirror(0x2205,25);
    Check(cache.ForDraw({},bank,{},{}).pipeline.modeCull == 25, "direct MMIO update does not reuse captured fallback");
    for (auto field : kDrawScalarFields) {
        const auto value = 0xc0000000u | field.index;
        registers[field.index] = value; putMirror(field.index,value); cache.Observe(field.index,value);
    }
    auto explicitState = cache.ForDraw({},bank,{},{});
    auto legacyState = CaptureLegacyDrawState({},bank,{},{});
    for (auto field : kDrawScalarFields)
        Check(BitsAt(explicitState,field.offset) == BitsAt(legacyState,field.offset), "all scalar mappings match legacy adapter");
    registers[0x2205] = 0; cache.Observe(0x2205,0); putMirror(0x2205,13);
    Check(cache.ForDraw({},bank,{},{}).pipeline.modeCull == 13, "native-to-zero legacy fallback remains live");
    registers[0x2205] = 71; putMirror(0x2205,71); cache.Reset();
    Check(cache.ForDraw({},bank,{},{}).pipeline.modeCull == 71, "external invalidation reseeds from current state");
    Check(!explicitState.vertexConstantRevision.Tracked(), "uncovered direct constant writes cannot skip uploads by revision");
    std::cout << "native mesh and mixed draw-state contracts passed\n";
}
