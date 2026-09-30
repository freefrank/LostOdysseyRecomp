// Focused wait and handle lifetime contracts; no game assets.
#include "kernel/dispatcher_wait.h"
#include "kernel/handle_table.h"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
using namespace std::chrono_literals;
namespace w = kernel::wait;
static int checks = 0;
static void Check(bool result, const char* why) { ++checks; if (!result) throw std::runtime_error(why); }
static void Waits() {
    w::Event first(false,true), second(false,false);
    w::Target* pair[]{&first,&second};
    Check(w::Multiple(pair,true,0)==w::Timeout,"WaitAll incorrectly succeeded");
    Check(first.Wait(0)==w::Success,"WaitAll partially consumed auto-reset event");
    first.Set();
    const auto begin = std::chrono::steady_clock::now();
    Check(w::Multiple(pair,true,35)==w::Timeout,"finite WaitAll ignored timeout");
    Check(std::chrono::steady_clock::now()-begin < 500ms,"WaitAll exceeded common deadline");
    Check(first.IsSignaled(),"timed-out WaitAll consumed first event");
    second.Set();
    Check(w::Multiple(pair,true,0)==w::Success,"ready WaitAll failed");
    Check(first.Wait(0)==w::Timeout && second.Wait(0)==w::Timeout,"all signals not consumed once");
    w::Semaphore semaphore(1,2); w::Target* mixed[]{&semaphore,&second};
    Check(w::Multiple(mixed,true,0)==w::Timeout && semaphore.Wait(0)==w::Success,"partial semaphore decrement");
    auto any = std::async(std::launch::async,[&]{return w::Multiple(mixed,false,1000);});
    Check(semaphore.Release(1,nullptr),"release failed");
    Check(any.get()==0,"WaitAny failed to wake on semaphore");
    w::Mutant mutant(7); w::Target* locks[]{&mutant,&first};
    Check(w::Multiple(locks,true,0,7)==w::Timeout,"unready mutant/event pair succeeded");
    Check(!mutant.Release(8),"non-owner released mutant");
    Check(mutant.Release(7),"WaitAll partially increased recursion");
    Check(mutant.Wait(0,8)==w::Success,"mutant ownership retained after timeout");
    Check(mutant.Release(8),"mutant release failed");
    w::Target* duplicate[]{&first,&first};
    Check(w::Multiple(duplicate,true,0)==w::Invalid,"duplicate WaitAll deadlock");
    Check(w::Multiple({},true,0)==w::Invalid,"empty WaitAll accepted");
    w::Target* invalid[]{nullptr};
    Check(w::Multiple(invalid,true,0)==w::Invalid,"null wait target accepted");
    // Unrelated signals must never extend the deadline indefinitely.
    std::atomic<bool> stop{false};
    std::jthread noise([&]{while(!stop.load()) { w::changes.Notify(); std::this_thread::yield(); }});
    const auto start = std::chrono::steady_clock::now();
    const auto result = w::Multiple(pair,true,25);
    stop=true;noise.join();
    Check(result==w::Timeout && std::chrono::steady_clock::now()-start<500ms,"signal traffic extended timeout");
    w::Semaphore one(1,1); w::Event ready(true,true); w::Target* onePair[]{&one,&ready};
    auto a=std::async(std::launch::async,[&]{return w::Multiple(onePair,true,40);});
    auto b=std::async(std::launch::async,[&]{return w::Multiple(onePair,true,40);});
    auto ar=a.get(),br=b.get();
    Check((ar==w::Success && br==w::Timeout)||(br==w::Success && ar==w::Timeout),"multiple waiters double-consumed semaphore");
}
static void Handles() {
    std::atomic<int> destroyed{0};
    struct Object {std::atomic<int>* n; explicit Object(std::atomic<int>* value):n(value){} virtual ~Object(){++*n;}};
    kernel::HandleTable<Object> table;
    auto object=std::make_shared<Object>(&destroyed);
    table.Insert(10,object);object.reset();
    Check(table.Duplicate(10,20,std::make_shared<int>(20),false),"duplicate failed");
    Check(table.Close(10) && !table.Close(10),"double close accepted");
    auto operation=table.Acquire(20);
    Check(bool(operation) && !table.Acquire(10),"closed canonical/valid duplicate confused");
    table.ReferenceObject(10,operation);
    Check(table.Close(20) && destroyed==0,"close destroyed an in-flight object");
    operation.reset();
    Check(destroyed==0 && bool(table.AcquireObject(10)),"Ob reference lost on last handle close");
    table.DereferenceObject(10);
    Check(destroyed==1,"last reference not destroyed exactly once");
    table.Insert(30,std::make_shared<Object>(&destroyed));
    Check(table.Duplicate(30,40,std::make_shared<int>(40),true),"close-source duplicate failed");
    Check(!table.Acquire(30) && bool(table.Acquire(40)),"close-source ownership wrong");
    Check(!table.Duplicate(30,50,{},false),"invalid handle duplicate accepted");
    Check(table.Close(40) && destroyed==2,"duplicate leaked/double destroyed");
    // An acquired operation remains live across a close from another thread.
    table.Insert(60,std::make_shared<Object>(&destroyed));
    auto hold=table.Acquire(60);
    std::thread closer([&]{table.Close(60);});closer.join();
    Check(destroyed==2,"concurrent close invalidated operation");hold.reset();
    Check(destroyed==3,"operation reference leaked");
    // A duplicate handle's token can be reused while an earlier Acquire keeps
    // its old object alive. The close result must identify the entry actually
    // removed in the same table operation, not that earlier observation.
    for (bool duplicateAndClose : {false, true}) {
        auto old=std::make_shared<Object>(&destroyed);
        auto token=std::make_shared<int>(70);
        std::weak_ptr<int> oldToken=token;
        table.Insert(70,old,std::move(token));old.reset();
        auto observed=table.Acquire(70);
        Check(table.Close(70) && oldToken.expired(),"closed duplicate token was not released");
        auto replacement=std::make_shared<Object>(&destroyed);
        table.Insert(70,replacement,std::make_shared<int>(70));
        std::shared_ptr<Object> removed;
        if (duplicateAndClose) {
            Check(table.Duplicate(70,80,std::make_shared<int>(80),true,&removed),"reused-token close-source duplicate failed");
            Check(!table.Acquire(70) && table.Acquire(80)==replacement,"close-source duplicate did not transfer the replacement");
        } else {
            Check(table.Close(70,&removed) && !table.Acquire(70),"reused-token close failed");
        }
        Check(removed==replacement && removed!=observed,"close attributed the reused token to the previously acquired object");
        if (duplicateAndClose) Check(table.Close(80),"replacement duplicate close failed");
        std::weak_ptr<Object> lifetime=removed;
        replacement.reset();
        Check(!lifetime.expired(),"reported removed object did not remain alive for diagnostics");
        removed.reset();
        Check(lifetime.expired(),"removed-object diagnostic reference leaked");
    }
}
int main(int argc, char** argv) try {
    if (argc==2 && std::string_view(argv[1])=="--handles") {
        Handles();
        std::cout<<"PASS "<<checks<<" handle-table checks\n";
        return 0;
    }
    Check(argc==1,"usage: kernel_wait_handle_test [--handles]");
    Waits();Handles();
    std::cout<<"PASS "<<checks<<" wait/handle checks\n";
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
