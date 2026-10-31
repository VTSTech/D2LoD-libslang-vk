// d2linux audio host - the stream the D2Sound shim (M2) will feed.
//
// SDL3's audio model is device streams, not legacy callbacks: open a stream
// at the game's native output shape and push mixed samples in. The scaffold
// pushes silence; the M2 D2Sound shim replaces the feeder only.
#include <SDL3/SDL.h>

#include <cstdio>

#include "d2_linux_audio.h"

namespace d2linux {
namespace {

SDL_AudioStream* g_stream = nullptr;

// Silence feeder. additional_amount = bytes the device wants; push zeros in
// chunks. (M2: D2Sound's mixed S16 stereo output goes here instead.)
void SDLCALL SilenceFeeder(void* /*userdata*/, SDL_AudioStream* stream,
                           int additional_amount, int /*total_amount*/) {
    static const Uint8 kSilence[8192] = {0};
    while (stream && additional_amount > 0) {
        const int n = additional_amount > (int)sizeof(kSilence)
                          ? (int)sizeof(kSilence)
                          : additional_amount;
        if (!SDL_PutAudioStreamData(stream, kSilence, n)) {
            return;
        }
        additional_amount -= n;
    }
}

}  // namespace

bool Audio_Init() {
    // Retail D2Sound mixes to 22.05 kHz S16 stereo; keep that shape so the
    // M2 shim feeds real samples without a rate/gain redesign.
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = 22050;

    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                         SilenceFeeder, nullptr);
    if (!g_stream) {
        return false;
    }
    if (!SDL_ResumeAudioStreamDevice(g_stream)) {
        SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
        return false;
    }
    return true;
}

void Audio_Shutdown() {
    if (g_stream) {
        SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
    }
}

}  // namespace d2linux
