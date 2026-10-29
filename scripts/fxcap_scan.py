#!/usr/bin/env python3
"""fxcap_scan.py — profile a fxparity capture (spike/fxcap.h format, LE u32 stream).

Answers the Stage-2 '8332/other texel formats' clause: which texture formats,
dims, LODs and draw shapes does the real field capture actually contain.
"""
import struct
import sys
from collections import Counter

MAGIC = 0x46584350
EV_FRAME, EV_CLEAR, EV_PALETTE, EV_TEXDOWN, EV_TEXSRC, EV_BLEND = 1, 2, 3, 4, 5, 6
EV_CONSTANT, EV_COLORFILL, EV_CHROMAKEY, EV_DRAWFAN, EV_DRAWPTRS = 7, 8, 9, 10, 11
EV_POINT, EV_LINE = 12, 13
NAMES = {1: "FRAME", 2: "CLEAR", 3: "PALETTE", 4: "TEXDOWN", 5: "TEXSRC",
         6: "BLEND", 7: "CONSTANT", 8: "COLORFILL", 9: "CHROMAKEY",
         10: "DRAWFAN", 11: "DRAWPTRS", 12: "POINT", 13: "LINE"}
FMT = {0x0: "RGB332", 0x1: "YIQ422", 0x2: "A8", 0x3: "I8", 0x4: "AI44",
       0x5: "P_8", 0x6: "rsvd6", 0x7: "rsvd7", 0x8: "ARGB8332", 0x9: "AYIQ8422",
       0xA: "RGB565", 0xB: "ARGB1555", 0xC: "ARGB4444", 0xD: "AI88", 0xE: "AP88"}


def main(path):
    w = open(path, "rb").read()
    assert len(w) % 4 == 0, "not word aligned"
    words = struct.unpack("<%dI" % (len(w) // 4), w)
    assert words[0] == MAGIC, "bad magic"
    i, n = 2, len(words)
    tags = Counter()
    fmts = Counter()
    dims = Counter()
    blends = Counter()
    shapes = Counter()
    frames = 0
    maxfan = 0
    maxptrs = 0
    texbytes = 0
    while i < n:
        tag = words[i]; nw = words[i + 1]; i += 2
        payload = words[i:i + nw]; i += nw
        tags[tag] += 1
        if tag == EV_FRAME:
            frames += 1
        elif tag == EV_TEXDOWN:
            fmt = payload[4]
            fmts[fmt] += 1
            texbytes += payload[5]
            dims[(payload[6], payload[7], payload[2], payload[3])] += 1
            # blob trailer: u32 byte_count + ceil(bytes/4) padded words
            nbytes = words[i]; i += 1
            i += (nbytes + 3) // 4
        elif tag == EV_BLEND:
            blends[(payload[0], payload[1])] += 1
        elif tag == EV_DRAWFAN:
            c = payload[0]; maxfan = max(maxfan, c); shapes[("fan", c)] += 1
        elif tag == EV_DRAWPTRS:
            c = payload[1]; maxptrs = max(maxptrs, c); shapes[("ptrs", payload[0], c)] += 1
    print("events=%d frames=%d texbytes=%d" % (sum(tags.values()), frames, texbytes))
    print("-- event tags --")
    for t, c in sorted(tags.items()):
        print("  %-10s %d" % (NAMES.get(t, "?"), c))
    print("-- texture formats (GR_TEXFMT_*) --")
    for f, c in sorted(fmts.items()):
        print("  0x%-2x %-10s %d downloads" % (f, FMT.get(f, "?"), c))
    print("-- texture dims (w x h, lod, aspect) top 12 --")
    for (tw, th, lod, asp), c in dims.most_common(12):
        print("  %3dx%-3d lod=%-2d aspect=%-2d  x%d" % (tw, th, lod, asp, c))
    print("-- distinct dims classes: %d" % len(dims))
    print("-- blend pairs (sf,df) --")
    for (sf, df), c in blends.most_common():
        print("  (%u,%u)  x%d" % (sf, df, c))
    print("-- draw shapes top 12 (max fan=%d, max ptrs=%d) --" % (maxfan, maxptrs))
    for k, c in shapes.most_common(12):
        print("  %-18s x%d" % (str(k), c))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/tmp/fxparity.cap")
