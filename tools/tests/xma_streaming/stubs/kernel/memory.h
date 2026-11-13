#pragma once
struct FixtureMemory {
    std::array<uint8_t,1024*1024> bytes{};
    std::array<uint8_t,0x4000> registers{};
    void* Translate(uint32_t address) {
        if(address>=0x7fea0000 && address<0x7fea4000) return registers.data()+address-0x7fea0000;
        const auto offset=address&0x1fffffff;
        if(offset>=bytes.size()) std::abort();
        return bytes.data()+offset;
    }
} inline g_memory;
struct FixtureAllocator {
    int physicalRegion=0;
    uint32_t Alloc(int,uint32_t,uint32_t) { return 0x1000; }
} inline g_pageAllocator;
