// vanilla-plus d2boot bridge - see VanillaPlusSound.h. Fork-owned TU: compiled
// into the D2Sound module, where SOUND_GetEAXEnabledFlag's module-private
// declaration is visible; the boot verdict in Game/Main.cpp rides this wrapper.
#include "Source/D2Sound/VanillaPlusSound.h"
#include "Source/D2Sound/D2Sound.h"

DWORD __stdcall VPSOUND_GetEAXEnabledFlag() {
    return D2Sound::SOUND_GetEAXEnabledFlag();
}
