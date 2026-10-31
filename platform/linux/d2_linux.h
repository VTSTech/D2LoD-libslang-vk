// d2linux - native Linux host layer (platform/linux).
//
// Shared declarations for the host skeleton. Milestone 0 (this tree): the
// host opens a window, presents an 8-bit palettized INDEX8 frame through the
// same surface/palette presentation model the game's pipeline uses, and
// streams silence through the audio path. NO game translation units are in
// this build yet - the port plan, shim inventory and milestone ladder live in
// docs/linux-port.md.
//
// Self-identification follows the PE line's d2boot convention: the banner
// names the build id so a console capture identifies the binary.
#pragma once

namespace d2linux {

struct HostOptions {
    bool smoke = false;       // --smoke: dummy drivers, a few frames, exit 0
    int  smoke_frames = 3;
    int  window_w = 800;
    int  window_h = 600;
};

// Returns false on a bad argument (usage already printed).
bool ParseArgs(int argc, char** argv, HostOptions* out);

// d2boot-style self-identification on stdout.
void PrintBanner(const HostOptions& opts);

}  // namespace d2linux
