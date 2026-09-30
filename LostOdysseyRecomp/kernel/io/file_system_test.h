#pragma once

// Deterministic synchronization points exist only in the storage test target.
#ifdef LO_STORAGE_TESTING
#include <cstdint>
namespace file_system_test
{
enum class Stage { HandleAcquired, IoLockAcquired, TransferDone, CompletionPublished, IoLockWaiting, BeforeCompletion };
using Hook = void (*)(Stage, uint32_t);
void SetHook(Hook hook);
}
#endif
