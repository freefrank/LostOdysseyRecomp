#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include "shader/position_evidence.h"
#include "taa_binding_evidence.h"
#include "collection_diagnostics.h"
namespace gpu::taa_collection {
// -1 undecided, 0 declined, 1 explicitly enabled. Separate from graphics previews.
void Initialize();
// Exit-only, nonblocking. Does not change the stored consent preference.
void Shutdown();
// Request one bounded background flush after an F1 capture. Never enables consent.
void RequestUpload();
int Consent();
bool SetConsent(bool enabled);
const wchar_t* Message(uint32_t language);
const wchar_t* Label(uint32_t language);
void PromptFirstRun(uint32_t language);
void SetDevice(bool vulkan, const std::string& name, uint64_t driver);
bool Enabled();
uint64_t ConsentEpoch() noexcept;
bool WantBinding() noexcept;
void ObserveBinding(const binding::Record& record, uint64_t consentEpoch, uint64_t frame) noexcept;
void BeginDiagnosticsFrame(uint64_t frame) noexcept;
bool DiagnosticsActive() noexcept;
void EndDiagnosticsFrame(uint64_t frame, const diagnostics::Frame& record) noexcept;
// Original guest microcode; count is in uint32_t words, hash is renderer byte FNV.
// Independent of TAA selection; only the existing collection consent enables it.
void ObserveProgram(bool vertex, uint64_t hash, const uint32_t* words, size_t count);
void Observe(uint64_t frame, uint64_t vs, uint64_t ps, uint32_t width, uint32_t height,
             int slot, uint32_t candidates, uint32_t flags, uint32_t rejection,
             const position_evidence::Summary& position, uint32_t guards);
}
