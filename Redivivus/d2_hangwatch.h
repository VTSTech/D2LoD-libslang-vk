// [vanilla-plus hangwatch] Freeze diagnostic - the crash handler cannot see a
// HANG. When the main loop stops beating for >12 s, a watcher thread suspends
// every thread, samples EIP + an EBP frame walk for each, resolves the
// addresses against d2exe.map (next to d2.exe, when present) and appends the
// result to d2boot.log - the same single file d2boot already produces.
// Read-only observer: it takes no game lock, holds no game state, and only
// ever writes the log.
#pragma once

extern "C" {
// Call once from the main thread at boot (after d2boot init). Arms the
// watcher thread; evaluation starts at the first heartbeat so the long
// boot stages (MPQ load, video setup) cannot false-positive.
void D2HANGWATCH_Start(void);

// One call per main-loop iteration (MessageGameLoop covers the out-of-game
// loops, the SP connect wait, the loading-screen wait and the in-game loop,
// which all run under it). Two volatile stores - safe at 25 fps.
void D2HANGWATCH_Heartbeat(void);
}
