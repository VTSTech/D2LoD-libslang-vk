#include "g3x_state.h"
#include "g3x_log.h"

#include <cstdio>

G3xState g_st;

void g3x_state_note_u32(const char* slot, unsigned value) {
    char tag[40];
    _snprintf(tag, sizeof(tag) - 1, "st:%s=%u", slot, value);
    tag[sizeof(tag) - 1] = '\0';
    g3x_log_once(tag, "%s -> %u (0x%08x)", slot, value, value);
}

void g3x_state_note_f32(const char* slot, float value) {
    char tag[40];
    _snprintf(tag, sizeof(tag) - 1, "st:%s=%.3f", slot, value);
    tag[sizeof(tag) - 1] = '\0';
    g3x_log_once(tag, "%s -> %.4f", slot, value);
}
