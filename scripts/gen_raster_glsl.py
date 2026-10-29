#!/usr/bin/env python3
"""gen_raster_glsl.py — Route-B structural single-source (ROUTE-B.md §4, §7 Stage 2).

Emits spike/routeb.comp FROM the math in src/g3x_raster_core.h. The kernel is
no longer a free-hand mirror: every math-critical line is EXTRACTED from the
core header at generation time. Where the kernel must deviate structurally
(heap addressing instead of pointers, switch -> if-chain, precise qualifiers,
tile slicing), the deviation is an explicit per-line override in this script —
an auditable map, not a silent rewrite.

Modes:
  gen_raster_glsl.py                 regenerate spike/routeb.comp in place
  gen_raster_glsl.py --check         byte-diff the emitted kernel against the
                                     committed one; nonzero exit + diff on any
                                     drift (the CI gate — a core edit that did
                                     not regenerate the kernel is RED)
  gen_raster_glsl.py --stdout        print the kernel (no files touched)

Pipeline per extracted line:  override (exact match) -> transforms (regex)
-> lint (C++-isms must be gone). A line that survives neither an override nor
the transforms fails the build LOUDLY, naming the unit and the line — a core
edit always forces a conscious visit here.

The bit-exactness contract itself is still the CI hash gate (corpus CPU pin +
Lavapipe GPU run + fxparity replay); this generator only guarantees the two
sources cannot silently diverge in the first place.
"""

import argparse
import re
import sys

CORE = "src/g3x_raster_core.h"
KERNEL = "spike/routeb.comp"


# ---- framework ---------------------------------------------------------------

class Unit:
    """One extraction unit: a line span of the core, mapped into kernel lines.

    start/end     anchored regexes for the span (end is INCLUSIVE)
    overrides     exact core line -> list of kernel lines (None drops the line;
                  a line matched here is trusted verbatim — it IS the audit map)
    transforms    ordered (pattern, repl) regexes applied to non-overridden lines
    lint          extra forbidden patterns for this unit (beyond the global set)
    """

    def __init__(self, name, start, end, overrides=None, transforms=None, lint=None):
        self.name = name
        self.start = start
        self.end = end
        self.overrides = overrides or {}
        self.transforms = transforms or []
        self.lint = lint or []


GLOBAL_LINT = [
    (r"\bunsigned\b", "C++ 'unsigned'"),
    (r"\buint32_t\b", "C++ 'uint32_t'"),
    (r"\bst\.", "Host accessor 'st.'"),
    (r"\(size_t\)", "C++ cast '(size_t)'"),
    (r"\bceilf\b", "C++ 'ceilf'"),
    (r"\bRVert\b", "C++ type 'RVert'"),
    (r"\bTexView\b", "C++ type 'TexView'"),
    (r"\bpmult\b", "CPU LUT 'pmult'"),
    (r"\btdata\b", "CPU pointer 'tdata'"),
    (r"\bvarying_vc\b", "CPU name 'varying_vc'"),
    (r"\(int\)", "C-style cast '(int)'"),
    (r"\d+\.0*f\b", "C float suffix"),
]


def load_core(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read().split("\n")


def extract_span(lines, unit):
    s = e = None
    for i, ln in enumerate(lines):
        if s is None and re.search(unit.start, ln):
            s = i
        elif s is not None and re.search(unit.end, ln):
            e = i
            break
    if s is None or e is None:
        raise SystemExit("gen: unit '%s': anchors not found in %s "
                         "(the core header drifted — update the unit)"
                         % (unit.name, CORE))
    return lines[s:e + 1]


def run_unit(unit, core_lines):
    src = extract_span(core_lines, unit)
    out = []
    for ln in src:
        if ln in unit.overrides:          # overrides win over everything
            rep = unit.overrides[ln]
            if rep:
                out.extend(rep)
            continue
        if ln.strip() == "":
            continue          # blank placement is template business, never extracted
        t = ln
        for pat, repl in unit.transforms:
            t = re.sub(pat, repl, t)
        bad = [why for pat, why in GLOBAL_LINT + unit.lint if re.search(pat, t)]
        if bad:
            raise SystemExit(
                "gen: unit '%s': line did not resolve to GLSL\n  core: %s\n  got: %s\n  offender: %s\n"
                "  add an explicit override or fix the transforms — never let it pass silently"
                % (unit.name, ln, t, ", ".join(bad)))
        if t.strip() == "" and ln.strip() != "":
            continue
        out.append(t)
    return out


# ---- units: the blend + edge math (verbatim mirrors) -------------------------

U_BLEND_CLASS = Unit(
    "blend_class_of",
    start=r"^inline BlendClass blend_class_of",
    end=r"^\}",
    overrides={
        "inline BlendClass blend_class_of(unsigned sf, unsigned df) {": [
            "uint blend_class_of(uint sf, uint df) {"],
        "    if      (sf == 4 && df == 0) return BC_OPAQUE;":
            ["    if      (sf == 4u && df == 0u) return 0u;   // BC_OPAQUE"],
        "    else if (sf == 4 && df == 4) return BC_ADD;":
            ["    else if (sf == 4u && df == 4u) return 1u;   // BC_ADD"],
        "    else if (sf == 1 && df == 5) return BC_LERP;":
            ["    else if (sf == 1u && df == 5u) return 2u;   // BC_LERP"],
        "    else if (sf == 0 && df == 2) return BC_MUL;":
            ["    else if (sf == 0u && df == 2u) return 3u;   // BC_MUL"],
        "    else                         return BC_GEN;":
            ["    else                           return 4u;   // BC_GEN"],
    },
)

U_BLEND_GEN = Unit(
    "blend_gen",
    start=r"^template <class H> inline uint32_t blend_gen",
    end=r"^\}$",
    overrides={
        "template <class H> inline uint32_t blend_gen(const H& st, uint32_t dst, uint32_t src) {": [
            "uint blend_gen(uint sf, uint df, uint ca, uint dst, uint src) {"],
        "    const unsigned sf = st.blend_sf(), df = st.blend_df();": [],
        "    const unsigned s[3] = { src & 0xFF, (src >> 8) & 0xFF, (src >> 16) & 0xFF };": [
            "    uint s[3] = { src & 0xFFu, (src >> 8) & 0xFFu, (src >> 16) & 0xFFu };"],
        "    const unsigned d[3] = { dst & 0xFF, (dst >> 8) & 0xFF, (dst >> 16) & 0xFF };": [
            "    uint d[3] = { dst & 0xFFu, (dst >> 8) & 0xFFu, (dst >> 16) & 0xFFu };"],
        "    unsigned out[3];": ["    uint outv[3];"],
        "        switch (sf) {": [],
        "            case 0:  sc = 0; break;                          // ZERO":
            ["        if      (sf == 0u) sc = 0u;"],
        "            case 1:  sc = st.const_alpha(); break;           // SRC_ALPHA":
            ["        else if (sf == 1u) sc = ca;"],
        "            case 2:  sc = s[i]; break;                       // SRC_COLOR":
            ["        else if (sf == 2u) sc = s[i];"],
        "            case 4:  sc = 255; break;                        // ONE":
            ["        else if (sf == 4u) sc = 255u;"],
        "            case 5:  sc = 255 - st.const_alpha(); break;     // ONE_MINUS_SRC_ALPHA":
            ["        else if (sf == 5u) sc = 255u - ca;"],
        "            default: sc = 255; break;":
            ["        else               sc = 255u;"],
        "        }": [],
        "        switch (df) {": [],
        "            case 0:  dc = 0; break;                          // ZERO":
            ["        if      (df == 0u) dc = 0u;"],
        "            case 1:  dc = st.const_alpha(); break;           // SRC_ALPHA":
            ["        else if (df == 1u) dc = ca;"],
        "            case 2:  dc = s[i]; break;                       // SRC_COLOR (dst * src)":
            ["        else if (df == 2u) dc = s[i];"],
        "            case 3:  dc = (dst >> 24) & 0xFF; break;         // DST_ALPHA":
            ["        else if (df == 3u) dc = (dst >> 24) & 0xFFu;"],
        "            case 4:  dc = 255; break;                        // ONE":
            ["        else if (df == 4u) dc = 255u;"],
        "            case 5:  dc = 255 - st.const_alpha(); break;":
            ["        else if (df == 5u) dc = 255u - ca;"],
        "            default: dc = 0; break;":
            ["        else               dc = 0u;"],
        "        unsigned v = (s[i] * sc + 255) >> 8;":
            ["        uint v = (s[i] * sc + 255u) >> 8;"],
        "        v += (d[i] * dc + 255) >> 8;":
            ["        v += (d[i] * dc + 255u) >> 8;"],
        "        out[i] = v > 255 ? 255 : v;":
            ["        outv[i] = v > 255u ? 255u : v;"],
        "    return out[0] | (out[1] << 8) | (out[2] << 16) | 0xFF000000u;": [
            "    return outv[0] | (outv[1] << 8) | (outv[2] << 16) | 0xFF000000u;"],
    },
    transforms=[(r"^        unsigned sc, dc;$", "        uint sc, dc;"),
                (r"^    for \(int i = 0; i < 3; \+\+i\) \{$", "    for (int i = 0; i < 3; ++i) {")],
)

U_BLEND_PX = Unit(
    "blend_px",
    start=r"^template <class H> inline uint32_t blend_px",
    end=r"^            return blend_gen\(st, dst, src\);$",
    overrides={
        "template <class H> inline uint32_t blend_px(const H& st, uint32_t dst, uint32_t src) {": [
            "uint blend_px(uint cls, uint sf, uint df, uint ca, uint dst, uint src) {"],
        "    switch (st.blend_class()) {": [],
        "        case BC_OPAQUE:": ["    if (cls == 0u) {                                   // BC_OPAQUE"],
        "            return src | 0xFF000000u;": ["        return src | 0xFF000000u;"],
        "        case BC_ADD: {": ["    } else if (cls == 1u) {                            // BC_ADD"],
        "        case BC_LERP: {": ["    } else if (cls == 2u) {                            // BC_LERP"],
        "            const unsigned a = st.const_alpha(), ia = 255u - a;":
            ["        uint a = ca, ia = 255u - a;"],
        "        case BC_MUL: {": ["    } else if (cls == 3u) {                            // BC_MUL"],
        "        default:": ["    } else {                                           // BC_GEN"],
        "            return blend_gen(st, dst, src);":
            ["        return blend_gen(sf, df, ca, dst, src);"],
        "        }": [],
    },
    transforms=[(r"^            ", "        "), (r"\bunsigned ", "uint ")],
)

U_OWNS_EDGE = Unit(
    "owns_edge",
    start=r"^inline bool owns_edge",
    end=r"^\}$",
    overrides={
        "inline bool owns_edge(const RVert& p, const RVert& q) {": ["bool owns_edge(V p, V q) {"],
    },
    transforms=[(r"^    const float dy = q\.y - p\.y;$", "    float dy = q.y - p.y;"),
                (r"0\.0f", "0.0")],
)

U_INSIDE_EXACT = Unit(
    "inside_exact",
    start=r"^inline bool inside_exact",
    end=r"^\}$",
    overrides={
        "inline bool inside_exact(int x, float py, const RVert& A, const RVert& B, const RVert& C,": [
            "bool inside_exact(int x, float py, V A, V B, V C, float inv,",
            "                  bool own0, bool own1, bool own2,",
            "                  out float wa, out float wb, out float wc) {"],
        "                         float inv, bool own0, bool own1, bool own2,": [],
        "                         float& wa, float& wb, float& wc) {": [],
        "    const float px = (float)x + 0.5f;": ["    float px = float(x) + 0.5;"],
    },
    transforms=[(r"0\.0f", "0.0")],
)

# ---- units: the triangle row math (do_tri) -----------------------------------
# Precise-qualifier rule: every float that feeds a stored pixel's math is
# `precise` in the kernel. The maps below insert exactly the qualifier set the
# committed kernel carries; a wider/narrower set shows up as a --check diff.

P = "precise "

U_TRI_HEAD = Unit(
    "tri_head",
    start=r"^inline void raster_tri",
    end=r"^    const TexView t = textured \? st\.tex_view\(\) : TexView\{\};$",
    overrides={
        "inline void raster_tri(H& st, const RVert& A0, const RVert& B0, const RVert& C0,": [],
        "                       float tsx, float tsy) {": [],
        "    if (!st.px()) return;": [],
        "    float area = (B0.x - A0.x) * (C0.y - A0.y) - (B0.y - A0.y) * (C0.x - A0.x);": [
            "    " + P + "float area = (B0.x - A0.x) * (C0.y - A0.y) - (B0.y - A0.y) * (C0.x - A0.x);"],
        "    RVert A = A0, B = B0, C = C0;": ["    V A = A0, B = B0, C = C0;"],
        "    const float inv = 1.0f / area;": ["    const " + P + "float inv = 1.0 / area;"],
        "    const bool textured = !st.color_fill() && st.has_tex();": [
            "    const bool textured = (color_fill == 0u) && has_tex;"],
        "    if (!textured && !st.color_fill()) return;":
            ["    if (!textured && color_fill == 0u) return;"],
        "    const TexView t = textured ? st.tex_view() : TexView{};": [],
    },
    transforms=[(r"0\.0f", "0.0")],
)

U_TRI_BBOX = Unit(
    "tri_bbox",
    start=r"^    float minx = A\.x < B\.x",
    end=r"^    if \(bx0 >= bx1 \|\| by0 >= by1\) return;$",
    transforms=[
        (r"^    float (minx|miny|maxx|maxy) = ", lambda m: "    " + P + "float " + m.group(1) + " = "),
        (r"\(int\)minx;", "int(minx);"),
        (r"\(int\)miny;", "int(miny);"),
        (r"\(int\)maxx \+ 1;", "int(maxx) + 1;"),
        (r"\(int\)maxy \+ 1;", "int(maxy) + 1;"),
        (r"\(int\)st\.width\(\)", "int(pw)"),
        (r"\(int\)st\.height\(\)", "int(ph)"),
    ],
)

U_TRI_OWN = Unit(
    "tri_own",
    start=r"^    bool own0 = owns_edge\(A, B\);",
    end=r"^    if \(swapped\) \{ own0 = !own0; own1 = !own1; own2 = !own2; \}$",
    overrides={
        "    bool own0 = owns_edge(A, B);         // wc == 0 -> pixel on edge A->B":
            ["    bool own0 = owns_edge(A, B);"],
        "    bool own1 = owns_edge(B, C);         // wa == 0 -> pixel on edge B->C":
            ["    bool own1 = owns_edge(B, C);"],
        "    bool own2 = owns_edge(C, A);         // wb == 0 -> pixel on edge C->A":
            ["    bool own2 = owns_edge(C, A);"],
    },
)

U_TRI_ATTRS = Unit(
    "tri_attrs",
    start=r"^    // constant-fill source \(colorCombine mode 2\)$",
    end=r"^    \};$",
    overrides={
        "    // constant-fill source (colorCombine mode 2)": [],
        "    // vertex-color mode: uniform (multiplied-palette LUT) or per-pixel interp":
            [""],
        "    const uint32_t fill_src = const_fill_rgb(st);":
            ["    const uint fill_src = fill_rgb;"],
        "    const bool uniform_vc = (A.argb == B.argb && B.argb == C.argb);":
            ["    const bool uniform_vc = (A.argb == B.argb && B.argb == C.argb);"],
        "    if (textured && uniform_vc && !st.color_fill()) {": [],
        "        st.ensure_pal_mult(A.argb & 0x00FFFFFFu);": [],
        "    }": [],
        "    float ac[3] = { (float)(A.argb & 0xFFu), (float)((A.argb >> 8) & 0xFFu), (float)((A.argb >> 16) & 0xFFu) };":
            ["    float ac[3] = { float(A.argb & 0xFFu), float((A.argb >> 8) & 0xFFu), float((A.argb >> 16) & 0xFFu) };"],
        "    float bc[3] = { (float)(B.argb & 0xFFu), (float)((B.argb >> 8) & 0xFFu), (float)((B.argb >> 16) & 0xFFu) };":
            ["    float bc[3] = { float(B.argb & 0xFFu), float((B.argb >> 8) & 0xFFu), float((B.argb >> 16) & 0xFFu) };"],
        "    float cc[3] = { (float)(C.argb & 0xFFu), (float)((C.argb >> 8) & 0xFFu), (float)((C.argb >> 16) & 0xFFu) };":
            ["    float cc[3] = { float(C.argb & 0xFFu), float((C.argb >> 8) & 0xFFu), float((C.argb >> 16) & 0xFFu) };"],
        "    // per-px +x slope of each interpolated quantity (d(wa)/dpx etc. — affine)": [],
        "    const float dudx = (A.u * (B.y - C.y) + B.u * (C.y - A.y) + C.u * (A.y - B.y)) * inv;":
            ["    const " + P + "float dudx = (A.u * (B.y - C.y) + B.u * (C.y - A.y) + C.u * (A.y - B.y)) * inv;"],
        "    const float dvdx = (A.v * (B.y - C.y) + B.v * (C.y - A.y) + C.v * (A.y - B.y)) * inv;":
            ["    const " + P + "float dvdx = (A.v * (B.y - C.y) + B.v * (C.y - A.y) + C.v * (A.y - B.y)) * inv;"],
        "    float dvc[3] = {": ["    " + P + "float dvc[3] = {"],
    },
)

U_TRI_ROW = Unit(
    "tri_row",
    start=r"^    for \(int y = by0; y < by1; \+\+y\) \{$",
    end=r"^        if \(empty \|\| lo > hi\) continue;$",
    overrides={
        "    for (int y = by0; y < by1; ++y) {": [],
        "        const float py = (float)y + 0.5f;":
            ["    const " + P + "float py = float(y) + 0.5;"],
        "        float lo = -3.0e38f, hi = 3.0e38f;":
            ["    " + P + "float lo = -3.0e38, hi = 3.0e38;"],
        "        bool empty = false;": ["    bool empty = false;"],
        "        // edge A->B  (its zero weight is wc)": [],
        "        // edge B->C  (wa)": [],
        "        // edge C->A  (wb)": [],
        "        {": ["    {"],
        "            const float dy = B.y - A.y;":
            ["        const " + P + "float dy = B.y - A.y;"],
        "            if (dy > 0.0f)      { const float xc = A.x + (B.x - A.x) * ((py - A.y) / dy); if (xc < hi) hi = xc; }":
            ["        if (dy > 0.0)      { const " + P + "float xc = A.x + (B.x - A.x) * ((py - A.y) / dy); if (xc < hi) hi = xc; }"],
        "            else if (dy < 0.0f) { const float xc = A.x + (B.x - A.x) * ((py - A.y) / dy); if (xc > lo) lo = xc; }":
            ["        else if (dy < 0.0) { const " + P + "float xc = A.x + (B.x - A.x) * ((py - A.y) / dy); if (xc > lo) lo = xc; }"],
        "            else {": ["        else {"],
        "                const float e = (B.x - A.x) * (py - A.y);":
            ["            const " + P + "float e = (B.x - A.x) * (py - A.y);"],
        "                if (e < 0.0f || (e == 0.0f && !own0)) empty = true;":
            ["            if (e < 0.0 || (e == 0.0 && !own0)) empty = true;"],
        "            const float dy = C.y - B.y;":
            ["        const " + P + "float dy = C.y - B.y;"],
        "            if (dy > 0.0f)      { const float xc = B.x + (C.x - B.x) * ((py - B.y) / dy); if (xc < hi) hi = xc; }":
            ["        if (dy > 0.0)      { const " + P + "float xc = B.x + (C.x - B.x) * ((py - B.y) / dy); if (xc < hi) hi = xc; }"],
        "            else if (dy < 0.0f) { const float xc = B.x + (C.x - B.x) * ((py - B.y) / dy); if (xc > lo) lo = xc; }":
            ["        else if (dy < 0.0) { const " + P + "float xc = B.x + (C.x - B.x) * ((py - B.y) / dy); if (xc > lo) lo = xc; }"],
        "                const float e = (C.x - B.x) * (py - B.y);":
            ["            const " + P + "float e = (C.x - B.x) * (py - B.y);"],
        "                if (e < 0.0f || (e == 0.0f && !own1)) empty = true;":
            ["            if (e < 0.0 || (e == 0.0 && !own1)) empty = true;"],
        "            const float dy = A.y - C.y;":
            ["        const " + P + "float dy = A.y - C.y;"],
        "            if (dy > 0.0f)      { const float xc = C.x + (A.x - C.x) * ((py - C.y) / dy); if (xc < hi) hi = xc; }":
            ["        if (dy > 0.0)      { const " + P + "float xc = C.x + (A.x - C.x) * ((py - C.y) / dy); if (xc < hi) hi = xc; }"],
        "            else if (dy < 0.0f) { const float xc = C.x + (A.x - C.x) * ((py - C.y) / dy); if (xc > lo) lo = xc; }":
            ["        else if (dy < 0.0) { const " + P + "float xc = C.x + (A.x - C.x) * ((py - C.y) / dy); if (xc > lo) lo = xc; }"],
        "                const float e = (A.x - C.x) * (py - C.y);":
            ["            const " + P + "float e = (A.x - C.x) * (py - C.y);"],
        "                if (e < 0.0f || (e == 0.0f && !own2)) empty = true;":
            ["            if (e < 0.0 || (e == 0.0 && !own2)) empty = true;"],
        "            }": ["        }"],
        "        }": ["    }"],
        "        if (empty || lo > hi) continue;": ["    if (empty || lo > hi) return;"],
    },
)

# boundary-spanning fixups advance/retreat/shrink/extend: verbatim except the
# 4-space dedent, ceilf->ceil, continue->return and the dropped for-comments.
DEDENT4 = (r"^(?=        )    ", "")

U_TRI_FIXUP = Unit(
    "tri_fixup",
    start=r"^        // candidate window: the rounded interval plus one pixel of slack per$",
    end=r"^            \+\+ex;$",
    overrides={
        "        // candidate window: the rounded interval plus one pixel of slack per": [],
        "        // side (division rounding is ~1e-4 px; the exact fixups below pin the": [],
        "        // true boundary). [sx, ex) candidates, ex exclusive.": [],
        "": [""],
    },
    transforms=[
        DEDENT4,
        (r"\(int\)ceilf\(([^)]*)\)", lambda m: "int(ceil(" + m.group(1) + "))"),
        (r"0\.5f", "0.5"),
        (r"\) continue;$", ") return;"),
        (r"\s+// (advance|retreat|shrink|extend).*$", ""),
    ],
    lint=[(r"0\.5f", "float suffix survived ceilf rewrite")],
)

U_TRI_V0 = Unit(
    "tri_v0",
    start=r"^        // attributes at the first covered pixel, then \+slope per pixel$",
    end=r"^        const float v0 = A\.v \* wa \+ B\.v \* wb \+ C\.v \* wc;$",
    overrides={
        "        // attributes at the first covered pixel, then +slope per pixel": [],
        "        const float u0 = A.u * wa + B.u * wb + C.u * wc;":
            ["    const " + P + "float u0 = A.u * wa + B.u * wb + C.u * wc;"],
        "        const float v0 = A.v * wa + B.v * wb + C.v * wc;":
            ["    const " + P + "float v0 = A.v * wa + B.v * wb + C.v * wc;"],
    },
    transforms=[DEDENT4],
)

# fill_span's head: the kernel derives the same quantities from the record +
# heap instead of host pointers; the vc/vr/vg/vb2 LUT multipliers ride along.
U_SPAN_HEAD = Unit(
    "span_head",
    start=r"^    const bool chroma = st\.chroma_on\(\) && t\.bpp == 1;$",
    end=r"^    float cr = vc0\[0\], cg = vc0\[1\], cb = vc0\[2\];$",
    overrides={
        "    const bool chroma = st.chroma_on() && t.bpp == 1;":
            ["    const bool chm    = (chroma_on == 1u) && (bpp == 1u);"],
        "    const uint8_t* hit = st.chroma_hit();": [],
        "    const uint32_t* pmult = st.pal_mult();": [],
        "    const uint32_t* pal = st.palette();": [],
        "    const uint8_t* tdata = t.data;": [],
        "    const unsigned tw = t.w, th = t.h;": [],
        "    float cr = vc0[0], cg = vc0[1], cb = vc0[2];":
            ["    const bool vary = (!uniform_vc) && (color_fill == 0u);",
             "    precise float u = u0, v = v0;",
             "    precise float cr = vc0[0], cg = vc0[1], cb = vc0[2];",
             "    // the CPU's uniform-VC LUT multipliers, exact from the vertex dword",
             "    const uint vc = A.argb & 0x00FFFFFFu;",
             "    const uint vr = vc & 0xFFu, vg = (vc >> 8) & 0xFFu, vb2 = (vc >> 16) & 0xFFu;"],
    },
)

# fill_span's pixel body, sliced: the pre-loop advance + xs/xe are template;
# every texel-decide/blend line is extracted. Heap addressing (byte_at /
# tex_off / pal_off / chroma_off) and the inline uniform-VC LUT formula are
# the audited deviations (the CPU memoizes pal_mult; the kernel cannot).
U_SPAN_BODY = Unit(
    "span_body",
    start=r"^    for \(int x = sx; x < ex; \+\+x\) \{$",
    end=r"^    \}$",
    overrides={
        "    for (int x = sx; x < ex; ++x) {": ["    for (int x = xs; x < xe; ++x) {"],
        "        } else if (t.bpp == 1 && tdata) {": ["        } else if (bpp == 1u) {"],
        "            const unsigned tx = (unsigned)(int)(u * tsx + 4096.0f) & (tw - 1);":
            ["            uint tx = uint(int(u * tsx + 4096.0)) & (tw - 1u);"],
        "            const unsigned ty = (unsigned)(int)(v * tsy + 4096.0f) & (th - 1);":
            ["            uint ty = uint(int(v * tsy + 4096.0)) & (th - 1u);"],
        "            const unsigned idx = tdata[(size_t)ty * tw + tx];":
            ["            uint idx = byte_at(tex_off + ty * tw + tx);"],
        "            if (chroma && hit[idx]) {":
            ["            if (chm && heap[chroma_off + idx] == 1u) {"],
        "            if (varying_vc) {":
            ["            const uint p = heap[pal_off + idx];",
             "            if (vary) {"],
        "                // colorCombine SCALE_OTHER x LOCAL: texel * interpolated color": [],
        "                unsigned vr = (unsigned)(0.5f + cr); if (vr > 255u) vr = 255u;":
            ["                uint q = uint(0.5 + cr); if (q > 255u) q = 255u;"],
        "                unsigned vg = (unsigned)(0.5f + cg); if (vg > 255u) vg = 255u;":
            ["                uint g8 = uint(0.5 + cg); if (g8 > 255u) g8 = 255u;"],
        "                unsigned vb = (unsigned)(0.5f + cb); if (vb > 255u) vb = 255u;":
            ["                uint b8 = uint(0.5 + cb); if (b8 > 255u) b8 = 255u;"],
        "                const uint32_t p = pal[idx];": [],
        "                src = pmult[idx];":
            ["                src = 0xFF000000u",
             "                    | ((((p & 0xFFu) * vr + 255u) >> 8))",
             "                    | (((((p >> 8) & 0xFFu) * vg + 255u) >> 8) << 8)",
             "                    | (((((p >> 16) & 0xFFu) * vb2 + 255u) >> 8) << 16);"],
        "        } else if (t.bpp == 2 && tdata) {": ["        } else if (bpp == 2u) {"],
        "            // GR_TEXFMT_RGB_565 (format 0xa) — the in-game Smacker video quads.": [],
        "            const uint16_t p5 = (uint16_t)(tdata[2 * ((size_t)ty * tw + tx)] |":
            ["            const uint ti = tex_off + 2u * (ty * tw + tx);",
             "            const uint p5 = byte_at(ti) | (byte_at(ti + 1u) << 8);"],
        "                                           (tdata[2 * ((size_t)ty * tw + tx) + 1] << 8));": [],
        "        row[x] = blend_px(st, row[x], src);":
            ["",
             "        const uint di = uint(y) * pw + uint(x);",
             "        const uint dst = out_px[di];",
             "        out_px[di] = blend_px(cls, sf, df, ca, dst, src);"],
        "            src = 0xFF000000u;   // other formats: still black, no chroma":
            ["            src = 0xFF000000u;"],
    },
    transforms=[(r"\buint32_t\b", "uint"),
                (r"\bconst unsigned ", "const uint "),
                (r"\bvarying_vc\b", "vary"),
                (r"\bvr\b", "q"), (r"\bvg\b", "g8"), (r"\bvb\b", "b8")],
)

# fill_rect_exec: bounds extracted verbatim (precise + int(ceil)), the row
# loop replaced by the tile slice; fill_src comes from the record.
U_RECT_BOUNDS = Unit(
    "rect_bounds",
    start=r"^    const float minx = xa < xb \? xa : xb, maxx = xa > xb \? xa : xb;$",
    end=r"^    int y1 = \(int\)ceilf\(maxy - 0\.5f\); if \(y1 > \(int\)h\) y1 = \(int\)h;$",
    overrides={
        "    const unsigned w = st.width(), h = st.height();": [],
    },
    transforms=[
        (r"^    const float minx = ", "    const " + P + "float minx = "),
        (r"^    const float miny = ", "    const " + P + "float miny = "),
        (r"\(int\)ceilf\(([^)]*)\)", lambda m: "int(ceil(" + m.group(1) + "))"),
        (r"0\.5f", "0.5"),
        (r"\(int\)w\b", "int(pw)"),
        (r"\(int\)h\b", "int(ph)"),
    ],
    lint=[(r"0\.5f", "float suffix survived ceilf rewrite")],
)

U_RECT_FILL = Unit(
    "rect_fill",
    start=r"^inline void fill_rect_exec",
    end=r"^    \}$",
    overrides={
        "template <class H>": [],
        "inline void fill_rect_exec(H& st, float xa, float xb, float ya, float yb) {": [],
        "    if (!st.px()) return;": [],
        "    const uint32_t fill_src = const_fill_rgb(st);":
            ["    const uint fill_src = recs[r0][10];         // const_fill_rgb, host-computed",
             "",
             "    int xs = x0 > tx0 ? x0 : tx0;",
             "    int xe = x1 < tx1 ? x1 : tx1;"],
        "    const float minx = xa < xb ? xa : xb, maxx = xa > xb ? xa : xb;": [],
        "    const float miny = ya < yb ? ya : yb, maxy = ya > yb ? ya : yb;": [],
        "    const unsigned w = st.width(), h = st.height();": [],
        "    int x0 = (int)ceilf(minx - 0.5f); if (x0 < 0) x0 = 0;": [],
        "    int y0 = (int)ceilf(miny - 0.5f); if (y0 < 0) y0 = 0;": [],
        "    int x1 = (int)ceilf(maxx - 0.5f); if (x1 > (int)w) x1 = (int)w;": [],
        "    int y1 = (int)ceilf(maxy - 0.5f); if (y1 > (int)h) y1 = (int)h;": [],
        "    for (int y = y0; y < y1; ++y) {": [],
        "        uint32_t* row = st.px() + (size_t)y * w;": [],
        "        for (int x = x0; x < x1; ++x) row[x] = blend_px(st, row[x], fill_src);":
            ["    for (int x = xs; x < xe; ++x) {",
             "        const uint di = uint(y) * pw + uint(x);",
             "        const uint dst = out_px[di];",
             "        out_px[di] = blend_px(cls, sf, df, ca, dst, fill_src);"],
    },
)


# ---- the kernel template (kernel-specific scaffolding, verbatim) --------------
# Every segment below is scaffolding the core header has no counterpart for:
# bindings/push constants, heap addressing helpers, record unpacking, tile
# bookkeeping, do_point (the point/line pixel op lives in g3x_frame.cpp, its
# math is blend_px + const_fill_rgb — both extracted above), and main().

SEG_HEAD = r'''// routeb.comp — the Route-B Stage 1 compute rasterizer: BINNED.
//
// GENERATED by scripts/gen_raster_glsl.py from src/g3x_raster_core.h — do not
// hand-edit the extracted math; edit the core (or this generator's audit map)
// and regenerate. `gen_raster_glsl.py --check` is the CI drift gate; the hash
// gates (corpus pin + Lavapipe + fxparity replay) remain the behavior proof.
//
// The GLSL rendering of src/g3x_raster_core.h (raster_tri + fill_span +
// fill_rect_exec + the point/line pixel op), one (TILE, row) per invocation:
//   workgroup (tiles_x, tiles_y) x local_size 32 = one invocation per tile
//   row. The invocation walks its tile's primitive bin in SUBMISSION ORDER
//   (the order the sink delivered the primitives in) and rasterizes each
//   primitive's intersection with its row — so every pixel has exactly one
//   writer and overlapping geometry blends in the CPU's painter order.
//   (Stage 0 was one (triangle, row) per invocation over a single-writer
//   corpus; ROUTE-B.md §3.2.)
//
// Bit-exactness under tiling (the Stage 1 crux): the CPU fills each row
// span in one pass, attributes stepping +slope per pixel FROM the span's
// first inside pixel. A tile only sees its x-slice of that span, so the
// kernel computes the span and its attribute origin EXACTLY as the CPU does
// (same interval math, same boundary fixups, same frame-wide clamps), then
// advances u/v/vc to its tile window with the identical add sequence before
// running the per-pixel body on the slice. Skipped pre-tile pixels skip
// only the texel fetch and the store — both side-effect-free — so every
// float the tile's pixels see is the bit the CPU saw. The row interval
// math itself is direct per-row evaluation (no cross-row accumulation), so
// rows parallelize freely.
//
// Mirrors, line-for-line (CPU side -> kernel):
//   blend_class_of / blend_px / blend_gen  -> same bodies
//   owns_edge / inside_exact               -> same bodies
//   tex_space_lod                          -> host-precomputed tsx/tsy
//   fill_span                              -> the tile-sliced pixel loop
//   raster_tri row loop                    -> do_tri (one row, write-filtered)
//   fill_rect_exec                         -> do_rect (one row, slice)
//   draw_point / draw_line per-pixel write -> do_point
// Deviations (value-identical, noted):
//   - minx/maxx via ternaries exactly as C++ (not min()/max() builtins)
//   - 565 texels compose from BYTES (the byte heap; LE compose == the CPU's
//     uint16 load on little-endian, and needs no alignment)
//   - the uniform-VC path computes the pal_mult LUT-entry formula inline
//     with the exact vertex dword — the CPU memoizes the same formula
//   - palette/chroma tables live in the shared heap at per-record word
//     offsets (content-versioned by the packer), not in dedicated bindings
//   - TEXEL WRAP: (int)(x + 4096) truncation == floor + 4096, the +4096
//     trick, exactly as fill_span does it
#version 460
precision highp float;
precision highp int;

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) readonly buffer VXYUV { vec4  v_xyuv[];  };
layout(std430, binding = 1) readonly buffer VARGB  { uint  v_argb[];  };
layout(std430, binding = 2) readonly buffer TRIS   { uvec4 tris[];    };
// 16 words per record: sf df const_alpha color_fill chroma_on tex_byte_off
// tex_w tex_h tex_lod tex_bpp fill_rgb tsx tsy pal_word_off chroma_word_off
// kind|has_tex<<8
layout(std430, binding = 3) readonly buffer RECS   { uint  recs[][16]; };
layout(std430, binding = 4) readonly buffer HEAP   { uint  heap[];    };
// rect/point prims, stride 2: {xa,xb,ya,yb | x,y} bits + {rec,0,0,0}
layout(std430, binding = 5) readonly buffer RRECTS { uvec4 rrects[];  };
// [NT counts][NT offsets][flat bin entries ((prim<<2)|kind)]
layout(std430, binding = 6) readonly buffer BINS   { uint  bins[];    };
layout(std430, binding = 7) buffer OUTPX { uint  out_px[];  };

layout(push_constant) uniform PC { uvec4 pc0; } pc;   // w, h, tiles_x, tiles_y

uint byte_at(uint i) { return (heap[i >> 2] >> ((i & 3u) * 8u)) & 0xFFu; }

struct V { float x, y, u, v; uint argb; };
V load_v(uint i) {
    vec4 f = v_xyuv[i];
    V r;
    r.x = f.x; r.y = f.y; r.u = f.z; r.v = f.w;
    r.argb = v_argb[i];
    return r;
}'''

L_BLEND_PX_HDR = r'''
// blend_px — the five-class switch, verbatim (g3xrc::blend_px).'''

L_TRI_OPEN = r'''
// ---- do_tri: one row of g3xrc::raster_tri, write-filtered to the tile ------
void do_tri(uint eidx, int y, int tx0, int tx1, uint pw, uint ph) {
    const uvec4 T   = tris[eidx];
    const V A0 = load_v(T.x), B0 = load_v(T.y), C0 = load_v(T.z);
    const uint r0 = T.w;   // record index — recs[][16] strides 16 words

    const uint sf        = recs[r0][0];
    const uint df        = recs[r0][1];
    const uint ca        = recs[r0][2];
    const uint color_fill= recs[r0][3];
    const uint chroma_on = recs[r0][4];
    const uint tex_off   = recs[r0][5];
    const uint tw        = recs[r0][6];
    const uint th        = recs[r0][7];
    const uint bpp       = recs[r0][9];
    const uint fill_rgb  = recs[r0][10];
    const precise float tsx = uintBitsToFloat(recs[r0][11]);
    const precise float tsy = uintBitsToFloat(recs[r0][12]);
    const uint pal_off    = recs[r0][13];
    const uint chroma_off = recs[r0][14];
    const bool has_tex    = (recs[r0][15] >> 8) != 0u;
    const uint cls = blend_class_of(sf, df);
'''

L_TRI_YRANGE = r'''    if (y < by0 || y >= by1) return;            // the CPU row loop's range
'''

L_TRI_ROWBODY = r'''
    // ---- the row body (the CPU's per-row loop, y fixed) ----------------------'''

L_SPAN_SLICE_HDR = r'''
    // ---- fill_span for this row, sliced to [tx0, tx1) ------------------------
    // The CPU runs the pixel loop over the WHOLE span; attributes step +slope
    // per pixel from sx. This tile only stores its slice, so: advance the
    // attributes from sx to the slice with the identical add sequence (the
    // skipped pixels' texel fetch and store are side-effect-free), then run
    // the exact per-pixel body inside the slice.'''

L_SPAN_PRELOOP = r'''
    for (int x = sx; x < ex; ++x) {
        if (x >= tx0) break;                    // reached the tile window
        u += dudx; v += dvdx;
        if (vary) { cr += dvc[0]; cg += dvc[1]; cb += dvc[2]; }
    }
    int xs = sx > tx0 ? sx : tx0;
    int xe = ex < tx1 ? ex : tx1;'''

L_RECT_OPEN = r'''
// ---- do_rect: one row of g3xrc::fill_rect_exec, sliced to the tile ---------
void do_rect(uint eidx, int y, int tx0, int tx1, uint pw, uint ph) {
    const uvec4 R = rrects[2u * eidx];
    const uint r0 = rrects[2u * eidx + 1u].x;
    const precise float xa = uintBitsToFloat(R.x);
    const precise float xb = uintBitsToFloat(R.y);
    const precise float ya = uintBitsToFloat(R.z);
    const precise float yb = uintBitsToFloat(R.w);

    // fill_rect_exec, verbatim forms'''

L_RECT_YRANGE = r'''    if (y < y0 || y >= y1) return;

    const uint sf = recs[r0][0];
    const uint df = recs[r0][1];
    const uint ca = recs[r0][2];
    const uint cls = blend_class_of(sf, df);'''

L_POINT_MAIN = r'''
// ---- do_point: one written pixel of draw_point / draw_line -----------------
void do_point(uint eidx, int y, int tx0, int tx1, uint pw) {
    const uvec4 R = rrects[2u * eidx];
    const uint r0 = rrects[2u * eidx + 1u].x;
    const precise float xv = uintBitsToFloat(R.x);
    const precise float yv = uintBitsToFloat(R.y);
    const int px = int(xv + 0.5), py = int(yv + 0.5);   // (int)(x + 0.5f), verbatim
    if (py != y) return;                                // another row's worker
    if (px < tx0 || px >= tx1) return;                  // another tile's worker
    // The sink only emits WRITTEN pixels (post clip), so px/py are in frame.
    const uint sf = recs[r0][0];
    const uint df = recs[r0][1];
    const uint cls = blend_class_of(sf, df);
    const uint di = uint(py) * pw + uint(px);
    const uint dst = out_px[di];
    out_px[di] = blend_px(cls, sf, df, recs[r0][2], dst, recs[r0][10]);
}

void main() {
    const uint pw = pc.pc0.x, ph = pc.pc0.y;
    const uint tiles_x = pc.pc0.z, tiles_y = pc.pc0.w;
    const uint NT = tiles_x * tiles_y;
    const uint tile = gl_WorkGroupID.y * tiles_x + gl_WorkGroupID.x;
    const int  y    = int(gl_WorkGroupID.y * 32u + gl_LocalInvocationID.x);
    if (y >= int(ph)) return;
    const int tx0 = int(gl_WorkGroupID.x * 32u);
    const int tx1 = tx0 + 32 < int(pw) ? tx0 + 32 : int(pw);

    const uint cnt = bins[tile];
    const uint off = bins[NT + tile];
    for (uint k = 0; k < cnt; ++k) {
        const uint e    = bins[2u * NT + off + k];
        const uint kind = e & 3u;
        const uint idx  = e >> 2;
        if      (kind == 0u) do_tri(idx, y, tx0, tx1, pw, ph);
        else if (kind == 1u) do_rect(idx, y, tx0, tx1, pw, ph);
        else                 do_point(idx, y, tx0, tx1, pw);
    }
}'''


def build():
    core = load_core(CORE)
    out = [SEG_HEAD, ""]
    out += run_unit(U_BLEND_CLASS, core) + [""]
    out += ["// blend_gen — the generic fallback, verbatim (g3xrc::blend_gen)."]
    out += run_unit(U_BLEND_GEN, core)
    out += L_BLEND_PX_HDR.split("\n")                # carries its leading blank
    out += run_unit(U_BLEND_PX, core) + ["    }", "}"]
    out += ["", "// owns_edge — verbatim (g3xrc::owns_edge)."]
    out += run_unit(U_OWNS_EDGE, core)
    out += ["", "// inside_exact — verbatim (g3xrc::inside_exact)."]
    out += run_unit(U_INSIDE_EXACT, core)
    out += L_TRI_OPEN.split("\n")                   # trailing blank = kernel L185
    out += run_unit(U_TRI_HEAD, core)                # area .. textured early-out
    out += [""]                                       # before the bbox block
    out += run_unit(U_TRI_BBOX, core)                # minx .. by1 clamps + if-return
    out += L_TRI_YRANGE.split("\n")               # trailing blank before own0
    out += run_unit(U_TRI_OWN, core) + [""]
    out += run_unit(U_TRI_ATTRS, core)
    out += L_TRI_ROWBODY.split("\n")                 # carries its leading blank
    out += run_unit(U_TRI_ROW, core) + [""]          # py .. if-return, blank
    out += run_unit(U_TRI_FIXUP, core) + ["    }"]   # sx/ex .. extend + close
    out += [""] + run_unit(U_TRI_V0, core)
    out += L_SPAN_SLICE_HDR.split("\n")              # carries its leading blank
    out += run_unit(U_SPAN_HEAD, core)
    out += L_SPAN_PRELOOP.split("\n")              # carries its leading blank
    out += run_unit(U_SPAN_BODY, core)
    out += ["}"] + L_RECT_OPEN.split("\n")
    out += run_unit(U_RECT_BOUNDS, core)
    out += L_RECT_YRANGE.split("\n")
    out += run_unit(U_RECT_FILL, core)
    out += ["}"] + L_POINT_MAIN.split("\n")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true",
                    help="byte-diff the emitted kernel against %s (CI gate)" % KERNEL)
    ap.add_argument("--stdout", action="store_true", help="print, do not write")
    args = ap.parse_args()

    text = build()
    if args.stdout:
        sys.stdout.write(text)
        return 0
    if args.check:
        with open(KERNEL, "r", encoding="utf-8") as f:
            committed = f.read()
        if committed == text:
            print("gen_raster_glsl: kernel matches the core (%d lines)" % text.count("\n"))
            return 0
        import difflib
        for ln in difflib.unified_diff(committed.split("\n"), text.split("\n"),
                                       "committed " + KERNEL, "generated",
                                       lineterm=""):
            print(ln)
        print("gen_raster_glsl: kernel OUT OF DATE vs %s — run "
              "`python3 scripts/gen_raster_glsl.py` and re-commit" % CORE)
        return 1
    with open(KERNEL, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("gen_raster_glsl: wrote %s (%d lines)" % (KERNEL, text.count("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
