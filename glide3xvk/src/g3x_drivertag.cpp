// glide3xvk — 0.4.1 implementation. See g3x_drivertag.h for the design.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "g3x_drivertag.h"

#include "g3x_log.h"
#include "g3x_overlay.h"

#include <cstdio>
#include <cstring>

namespace {

char s_tag[32];        // hard cap: the game sprintf()s into char[32]
bool s_patched = false;

struct Image {
    uint8_t*            base = nullptr;
    size_t              size = 0;
    IMAGE_NT_HEADERS32* nt   = nullptr;
};

bool image_of(Image& img) {
    const HMODULE exe = GetModuleHandleA(nullptr);
    if (!exe) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(
        reinterpret_cast<uint8_t*>(exe) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    img.base = reinterpret_cast<uint8_t*>(exe);
    img.size = nt->OptionalHeader.SizeOfImage;
    img.nt   = nt;
    return img.base && img.size;
}

// Count exact-byte matches; stop as soon as a second one shows up, because
// ambiguity means bail (we never guess which copy to patch).
size_t count_bytes(const Image& img, const void* pat, size_t len, size_t* first) {
    if (first) *first = size_t(-1);
    if (!len || len > img.size) return 0;
    const uint8_t c0 = *static_cast<const uint8_t*>(pat);
    size_t n = 0;
    for (size_t i = 0; i + len <= img.size; ++i) {
        if (img.base[i] != c0) continue;
        if (std::memcmp(img.base + i, pat, len) == 0) {
            if (n == 0 && first) *first = i;
            if (++n > 1) break;
        }
    }
    return n;
}

// Scan a run of PE sections for one 4-byte little-endian value. Returns true
// with `site` set when exactly one hit exists in the run; ambiguity fails.
bool find_imm_unique(const Image& img, const IMAGE_SECTION_HEADER* secs, unsigned nsec,
                     uint32_t value, uint8_t*& site, bool& ambiguous) {
    site = nullptr;
    ambiguous = false;
    for (unsigned i = 0; i < nsec; ++i, ++secs) {
        const size_t n = secs->Misc.VirtualSize;
        if (!n) continue;
        const uint8_t* s = img.base + secs->VirtualAddress;
        for (size_t o = 0; o + 4 <= n; ++o) {
            uint32_t v;
            std::memcpy(&v, s + o, 4);
            if (v != value) continue;
            if (site) { ambiguous = true; return false; }
            site = img.base + secs->VirtualAddress + o;
        }
    }
    return site != nullptr;
}

} // namespace

namespace g3x_drivertag {

const char* tag() { return s_tag; }

bool install() {
    static bool tried = false;
    if (tried) return s_patched;
    tried = true;

    // 1. the tag text — snprintf clamps it to the game's buffer size.
    std::snprintf(s_tag, sizeof(s_tag), "%s", g3x_overlay_tagline());

    Image img;
    if (!image_of(img)) {
        g3x_log("drivertag: Game.exe PE header unreadable -> box overlay stays");
        return false;
    }

    // 2. the renderer-name string pool as laid out by the 1.14d switch. The
    //    tight anchor names the whole pool run; the loose one survives minor
    //    layout drift. "Glide" is the tail of both.
    static const char kAnchor1[] = "Software\0DirectDraw\0Glide";
    static const char kAnchor2[] = "DirectDraw\0Glide";
    const size_t kGlideLen = 5;  // strlen("Glide") — the tail of both anchors
    size_t off = size_t(-1), anchor_len = 0;
    const size_t n1 = count_bytes(img, kAnchor1, sizeof(kAnchor1) - 1, &off);
    if (n1 == 1) {
        anchor_len = sizeof(kAnchor1) - 1;
    } else if (n1 > 1) {
        g3x_log("drivertag: renderer-name pool ambiguous (%u hits) -> box overlay stays",
                (unsigned)n1);
        return false;
    } else {
        const size_t n2 = count_bytes(img, kAnchor2, sizeof(kAnchor2) - 1, &off);
        if (n2 != 1) {
            g3x_log("drivertag: renderer-name pool not found (%u hits) -> box overlay stays",
                    (unsigned)n2);
            return false;
        }
        anchor_len = sizeof(kAnchor2) - 1;
    }
    const uint8_t* glide_str = img.base + off + anchor_len - kGlideLen;
    const uint32_t glide_va  = (uint32_t)(uintptr_t)glide_str;
    g3x_log("drivertag: pool found, \"Glide\" @0x%08X, searching the reference...",
            glide_va);

    // 3. exactly one 4-byte reference to that VA. Pass 0 covers executable
    //    sections (the switch's mov-imm in .text — the expected shape); pass 1
    //    falls back to the remaining sections for a pointer-table layout.
    const IMAGE_SECTION_HEADER* secs = IMAGE_FIRST_SECTION(img.nt);
    const unsigned nsec = img.nt->FileHeader.NumberOfSections;
    uint8_t* site = nullptr;
    bool ambiguous = false;
    for (unsigned pass = 0; pass < 2 && !site && !ambiguous; ++pass) {
        const IMAGE_SECTION_HEADER* sec = secs;
        for (unsigned i = 0; i < nsec; ++i, ++sec) {
            const bool exec = (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
            if ((pass == 0) != exec) continue;
            if (pass == 1 && (sec->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)) continue;
            uint8_t* hit = nullptr;
            bool amb = false;
            find_imm_unique(img, sec, 1, glide_va, hit, amb);
            if (amb || (hit && site)) { ambiguous = true; break; }
            if (hit) site = hit;
        }
    }
    if (!site) {
        g3x_log("drivertag: %s -> box overlay stays",
                ambiguous ? "reference to \"Glide\" ambiguous" : "no reference to \"Glide\" found");
        return false;
    }

    // 4. repoint — one 4-byte store on a page flipped writable for it.
    const uint32_t tag_va = (uint32_t)(uintptr_t)s_tag;
    DWORD oldp = 0;
    if (!VirtualProtect(site, sizeof(tag_va), PAGE_EXECUTE_READWRITE, &oldp)) {
        g3x_log("drivertag: VirtualProtect failed (gle=%lu) -> box overlay stays",
                (unsigned long)GetLastError());
        return false;
    }
    std::memcpy(site, &tag_va, sizeof(tag_va));
    DWORD tmp = 0;
    VirtualProtect(site, sizeof(tag_va), oldp, &tmp);
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(tag_va));
    s_patched = true;
    g3x_log("drivertag: \"Video:\" line patched @0x%08X -> \"%s\"",
            (unsigned)(uintptr_t)site, s_tag);
    return true;
}

} // namespace g3x_drivertag
