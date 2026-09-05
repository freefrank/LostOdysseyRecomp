#include <apu/xma_loop.h>
#include <cstdio>

int main()
{
    auto check = [](bool ok) { if (!ok) { std::puts("XMA loop regression failed"); return false; } return true; };
    // Live looping voice: 219 packets, end lies inside the last packet. Its
    // trailing frame bit advances to the next packet rather than exactly end.
    uint32_t read = 219 * 16384, count = 255;
    if (!check(apu::xma::RestartLoop(241976, 3580323, true, read, count) && read == 241976 && count == 255)) return 1;
    read = 17000; count = 2;
    if (!check(apu::xma::RestartLoop(32, 16800, false, read, count) && read == 32 && count == 1)) return 1;
    read = 16800;
    if (!check(apu::xma::RestartLoop(32, 16800, false, read, count) && count == 0)) return 1;
    read = 17000;
    if (!check(!apu::xma::RestartLoop(32, 16800, true, read, count) && read == 17000)) return 1;
    count = 255; read = 100;
    if (!check(!apu::xma::RestartLoop(32, 16800, false, read, count) && read == 100)) return 1;
    if (!check(!apu::xma::RestartLoop(100, 100, true, read, count))) return 1;
    std::puts("XMA loop regressions passed");
}
