#include <cpu/poll_wait.h>
#include <atomic>
#include <cassert>
#include <csetjmp>
#include <iostream>
#include <stdexcept>
#include <vector>

int main()
{
    using namespace poll_wait;
    std::vector<uint32_t> delays;
    auto fake = [&](uint32_t us) { delays.push_back(us); };
    assert(!ZeroDelay(fake));
    QueryResult(1, fake);
    assert(delays.empty());
    {
        Scope outer(Kind::Query);
        for (int i=0;i<32;++i) QueryResult(1,fake);
        assert(delays.empty());
        QueryResult(-1,fake);
        assert(delays.back()==50);
        for (int i=0;i<32;++i) QueryResult(1,fake);
        assert(delays.back()==200);
        for (int i=0;i<10000;++i) QueryResult(1,fake);
        assert(query.polls==65 && delays.back()==200);
        auto saved=query.polls;
        try { Scope inner(Kind::Query); QueryResult(1,fake); assert(query.polls==1); throw 7; }
        catch(int) {}
        assert(query.polls==saved);
        QueryResult(0,fake);
        assert(query.polls==0);
        QueryResult(2,fake);
        assert(query.polls==0);
        assert(!ZeroDelay(fake));
    }
    assert(!query.active && query.polls==0);
    GpuPollResult(1, fake);
    assert(delays.back()==200);
    {
        Scope outer(Kind::GpuPoll);
        const auto beforeWarmup = delays.size();
        for (int i=0;i<32;++i) GpuPollResult(1,fake);
        assert(delays.size()==beforeWarmup);
        GpuPollResult(1,fake);
        assert(delays.back()==50);
        QueryResult(1,fake);
        assert(!ZeroDelay(fake));
        for (int i=0;i<32;++i) GpuPollResult(1,fake);
        assert(delays.back()==kGpuPollDelayCapUs);
        for (int i=0;i<10000;++i) GpuPollResult(1,fake);
        assert(gpuPoll.polls==65 && delays.back()==kGpuPollDelayCapUs);
        GpuPollResult(0,fake);
        assert(gpuPoll.polls==0);
        GpuPollResult(2,fake);
        assert(gpuPoll.polls==0);
        auto saved=gpuPoll.polls;
        try { Scope inner(Kind::GpuPoll); GpuPollResult(1,fake); assert(gpuPoll.polls==1); throw 7; }
        catch(int) {}
        assert(gpuPoll.polls==saved && gpuPoll.active);
    }
    assert(!gpuPoll.active && gpuPoll.polls==0);
    {
        Scope scope(Kind::SharedValue);
        assert(ZeroDelay(fake));
        assert(sharedValue.polls==1 && !query.active && !gpuPoll.active);
    }
    assert(!sharedValue.active);
    RunScoped(Kind::Query, [&] {
        QueryResult(1,fake);
        try { RunScoped(Kind::Query, []{throw 3;}); } catch(int) {}
        assert(query.active && query.polls==1);
    });
    assert(!query.active);
    std::jmp_buf target;
    if (setjmp(target)==0)
        RunScoped(Kind::SharedValue, [&]{ std::longjmp(target,1); });
    ResetThread();
    assert(!query.active && !sharedValue.active && !gpuPoll.active);

    int armed=0,waited=0,cancelled=0,yielded=0;
    auto arm=[&](uint32_t us){++armed;assert(us==200);return true;};
    auto cancel=[&]{++cancelled;};auto yield=[&]{++yielded;};
    WaitOnce(200,arm,[&]{++waited;return true;},cancel,yield);
    assert(armed==1 && waited==1 && !cancelled && !yielded);
    WaitOnce(200,arm,[&]{++waited;return false;},cancel,yield);
    assert(armed==2 && waited==2 && cancelled==1 && yielded==1);
    WaitOnce(200,[](uint32_t){return false;},[&]{++waited;return true;},cancel,yield);
    assert(waited==2 && cancelled==1 && yielded==2);

    // Producer completion is still observed by fresh loads; per-thread scopes
    // cannot throttle another thread and a stop request is checked after a pause.
    std::atomic<int> ready{0}, started{0};
    std::atomic<bool> stop{false};
    std::atomic<int> completed{0};
    std::vector<std::thread> workers;
    for(int i=0;i<4;++i) workers.emplace_back([&,i]{
        Scope scope(i%2 ? Kind::Query : Kind::SharedValue);
        ++started;
        while(!stop.load() && ready.load()==0)
            if(i%2) QueryResult(1); else ZeroDelay();
        if(ready.load()==19) ++completed;
    });
    while(started.load()!=4) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ready.store(19);
    for(auto& w:workers) w.join();
    assert(completed==4 && !query.active && !sharedValue.active);
    auto start=std::chrono::steady_clock::now();
    std::thread waiter([&]{Scope scope(Kind::Query);while(!stop.load()) QueryResult(1);});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    stop.store(true);waiter.join();
    auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    assert(elapsed<1000); // Detect unbounded waits, not a scheduler latency promise.
    std::cout << "PASS poll result/reset/nesting/thread isolation/timer failure/progress/stop " << elapsed << "ms\n";
}
