#pragma once

#include <cstdint>

// Entry points of the battle intro skip (battle_intro_skip.cpp) for other hooks, and
// the prompt window state for the host overlay.
namespace battle_intro_skip
{
// Guest game thread. RequestBattle (sub_828278A0) accepted a request.
void NoteBattleRequest(uint32_t id, bool walkingDraw);
// Guest game thread. sub_82828698 rewrote the battle slot: that request is not random.
void NoteSlotRewritten();
// Guest game thread. encounter_defer holds the request while a field event has control.
void NoteRequestHeld();
// Guest game thread. The loader starts this battle request (sub_8231F5E0).
void NoteBattlePicked(uint32_t id);
// Guest game thread. encounter_defer dropped the held request for a scripted one.
void NoteRequestDropped();
// Guest game thread. The debug battle tour put this formation into the walking draw.
void NoteTourDraw(int32_t formation);
// Guest game thread, once per engine tick (after the world tick).
void EngineTick(uint8_t* base);
}

namespace battle_intro_skip::prompt
{
// Any thread. True while the skip window is open and the game thread reported it
// in the last PromptStaleMs.
bool Shown();
// Any thread. Milliseconds since the window opened (for the fade-in); 0 when not shown.
uint64_t ShownForMs();
// Any thread. The game's cancel button is A (the Confirmation button option B / A).
bool CancelIsA();
}
