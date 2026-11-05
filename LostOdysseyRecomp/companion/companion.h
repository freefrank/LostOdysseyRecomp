#pragma once

#include <cstdint>
#include <string>

// Dual-screen companion: a live, touchable panel on the second display of
// dual-screen Android handhelds such as the AYN Thor (inspired by the Eden Duo
// companions). The game thread samples player-facing state into an owned JSON
// snapshot; the Android Presentation (DualScreenCompanion.java) only ever reads
// that string and sends button presses back. It never touches guest memory.
namespace companion
{
// Engine thread only (the GEngine tick hook in debug/teleport.cpp).
void Tick(uint8_t* base);
// Any thread: the latest snapshot as JSON ("{}" before the first sample).
std::string StateJson();
// Any thread: actions requested from the panel, as a small JSON object.
// Returns false when the action is unknown or refused.
bool HandleAction(const std::string& json);
}
