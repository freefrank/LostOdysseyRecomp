#include <stdafx.h>
#include "poll_wait.h"

extern "C" PPC_FUNC(__imp__sub_823CF390);
extern "C" PPC_FUNC(__imp__sub_82322478);

// Retain the generated function, including register saves, return values and
// query ordering. Only its repeated not-ready probes can pause on the host.
PPC_FUNC(sub_823CF390)
{
    poll_wait::RunScoped(poll_wait::Kind::Query, [&] { __imp__sub_823CF390(ctx, base); });
}

// This guest loop repeatedly invokes Sleep(0) while a shared value exceeds its
// threshold. Keep its original reads and comparison; opt in only this call tree.
PPC_FUNC(sub_82322478)
{
    poll_wait::RunScoped(poll_wait::Kind::SharedValue, [&] { __imp__sub_82322478(ctx, base); });
}
