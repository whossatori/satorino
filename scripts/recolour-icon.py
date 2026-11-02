#!/usr/bin/env python3
# satorino: recolour the app icon to the fork colour (light purple #cba6f7,
# Catppuccin Mocha "mauve") and regenerate every icon format in place.
#
# Method: hue shift to the target hue with a power-law remap of saturation
# and luminance, anchored on the source's dominant saturated colour so that
# colour maps EXACTLY to the target. Power laws (not scale+shift) keep the
# mapping smooth and monotonic when the anchor is far from the target (the
# rasters are leafyrino's muted green, the SVG still chatterino blue).
# Neutrals (S < 2%: the grey shadow layer, white mask, black drop shadow)
# and alpha are untouched; only coloured pixels turn mauve.
#
# Masters:
#   - rasters: the largest PNG chunk inside resources/chatterino.icns
#     (1024x1024) - every output size is a LANCZOS resize of it
#   - resources/icon.svg: recoloured with its own anchor (first saturated
#     fill), because the SVG and the rasters diverged in the fork chain
#
# Outputs (file names unchanged - no case-only renames):
#   resources/icon.png          256x256  (Linux install/flatpak/AppImage)
#   resources/icon.ico          16..256  (exe icon, installer, tray :/icon.ico)
#   resources/chatterino.icns   16..1024 (macOS bundle icon)
#   resources/icon.svg          fills recoloured
#
# icns note: the original file's legacy RLE 'ic04'/'ic05' (16/32 px ARGB)
# chunks are replaced by PNG 'icp4'/'icp5' chunks - same sizes, the modern
# encoding macOS itself emits via iconutil. All other chunks keep their
# original types; the binary-plist 'info' chunk is copied verbatim.
#
# Re-runnable after an upstream sync replaces the icon files: anchor colours
# are auto-detected, nothing is hardcoded.
#
# Never use this on emotes/badges/avatars - it is for the app icon only.

import colorsys
import io
import math
import pathlib
import re
import struct
import sys

import numpy as np
from PIL import Image

REPO = pathlib.Path(__file__).resolve().parent.parent
RES = REPO / "resources"

TARGET_HEX = "#cba6f7"

# ---------------------------------------------------------------- helpers


def hex_to_rgb01(h):
    h = h.lstrip("#")
    return tuple(int(h[i : i + 2], 16) / 255.0 for i in (0, 2, 4))


def rgb01_to_hex(rgb):
    return "#" + "".join(f"{round(c * 255):02X}" for c in rgb)


TARGET_RGB = hex_to_rgb01(TARGET_HEX)
TARGET_H, TARGET_L, TARGET_S = colorsys.rgb_to_hls(*TARGET_RGB)


def make_mapper(anchor_s, anchor_l):
    """Return f(h, l, s) -> (r, g, b) with the anchor mapped to TARGET."""
    # power-law exponents: anchor^(g) = target  =>  g = ln(t)/ln(a)
    gl = (
        math.log(TARGET_L) / math.log(anchor_l)
        if 0.0 < anchor_l < 1.0
        else None
    )
    gs = (
        math.log(TARGET_S) / math.log(anchor_s)
        if 0.0 < anchor_s < 1.0
        else None
    )

    def f(h, l, s):
        if s < 0.02:  # neutral: keep greys / white / black
            return colorsys.hls_to_rgb(h, l, s)
        if gs is None:  # anchor fully saturated: scale down linearly
            s2 = s * TARGET_S
        else:
            s2 = s**gs
        l2 = l**gl if gl is not None else l * TARGET_L
        return colorsys.hls_to_rgb(TARGET_H, min(1.0, l2), min(1.0, s2))

    return f


def recolour_image(img, mapper):
    arr = np.asarray(img.convert("RGBA"), dtype=np.uint8)
    flat = arr.reshape(-1, 4)
    uniq, inv = np.unique(flat, axis=0, return_inverse=True)
    out = np.empty((len(uniq), 4), dtype=np.uint8)
    for i, (r, g, b, a) in enumerate(uniq):
        nr, ng, nb = mapper(*colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0))
        out[i] = (
            round(max(0.0, min(1.0, nr)) * 255.0),
            round(max(0.0, min(1.0, ng)) * 255.0),
            round(max(0.0, min(1.0, nb)) * 255.0),
            a,
        )
    return Image.fromarray(out[inv].reshape(arr.shape))


def find_anchor(img):
    """Dominant saturated colour (weighted by pixel count) -> (s, l, hex)."""
    arr = np.asarray(img.convert("RGBA"), dtype=np.uint8)
    flat = arr.reshape(-1, 4)
    uniq, counts = np.unique(flat, axis=0, return_counts=True)
    best, best_w = None, -1
    for (r, g, b, a), w in zip(uniq, counts):
        if a == 0:
            continue
        h, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
        if s < 0.15:
            continue
        if w > best_w:
            best_w, best = w, (s, l, (r, g, b))
    if best is None:
        sys.exit("no saturated colour found - is this already greyscale?")
    s, l, rgb = best
    print(f"  anchor {rgb01_to_hex(tuple(c / 255 for c in rgb))} "
          f"(S={s:.3f} L={l:.3f}, {best_w} px) -> {TARGET_HEX}")
    return s, l


# ------------------------------------------------------------------ icns


def parse_icns(data):
    assert data[:4] == b"icns"
    chunks, off = [], 8
    while off < len(data):
        typ = data[off : off + 4]
        ln = struct.unpack(">I", data[off + 4 : off + 8])[0]
        chunks.append((typ, data[off + 8 : off + ln]))
        off += ln
    return chunks


def build_icns(entries):
    body = bytearray()
    for typ, payload in entries:
        body += typ + struct.pack(">I", len(payload) + 8) + payload
    return b"icns" + struct.pack(">I", len(body) + 8) + bytes(body)


def png_bytes(img):
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


# ------------------------------------------------------------------ main


def main():
    icns_path = RES / "chatterino.icns"
    chunks = parse_icns(icns_path.read_bytes())

    # 1. master = largest PNG chunk
    master_png = max(
        (p for _, p in chunks if p[:8] == b"\x89PNG\r\n\x1a\n"), key=len
    )
    master = Image.open(io.BytesIO(master_png)).convert("RGBA")
    print(f"master raster: {master.size[0]}x{master.size[1]} from icns")

    # 2. anchor + recolour master
    print("recolouring rasters:")
    as_, al = find_anchor(master)
    mapper = make_mapper(as_, al)
    new_master = recolour_image(master, mapper)

    # verify the anchor really maps onto the target
    got = new_master.getpixel(
        tuple(c // 2 for c in new_master.size)
    )
    print(f"  centre pixel after recolour: #{got[0]:02X}{got[1]:02X}{got[2]:02X}")

    # 3. frames
    def frame(size):
        return new_master.resize((size, size), Image.Resampling.LANCZOS)

    sizes = {s: frame(s) for s in (16, 32, 64, 128, 256, 512, 1024)}

    # 4. icon.png
    sizes[256].save(RES / "icon.png", format="PNG")
    print("icon.png: 256x256 written")

    # 5. icon.ico (frames provided explicitly at every size)
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_frames = {s: frame(s) for s in ico_sizes}
    ico_frames[256].save(
        RES / "icon.ico",
        format="ICO",
        append_images=[ico_frames[s] for s in ico_sizes if s != 256],
        sizes=[(s, s) for s in ico_sizes],
    )
    print(f"icon.ico: {ico_sizes} written")

    # 6. chatterino.icns - same layout, PNG chunks recoloured, legacy
    #    RLE ic04/ic05 swapped for PNG icp4/icp5
    dim_of = {
        b"icp4": 16, b"icp5": 32, b"icp6": 128,
        b"ic11": 32, b"ic12": 64, b"ic07": 128,
        b"ic08": 256, b"ic13": 256, b"ic09": 512,
        b"ic14": 512, b"ic10": 1024,
    }
    entries = []
    swaps = []
    for typ, payload in chunks:
        if typ == b"info":
            entries.append((typ, payload))  # verbatim metadata
        elif typ in (b"ic04", b"ic05"):
            new_typ = b"icp4" if typ == b"ic04" else b"icp5"
            dim = 16 if typ == b"ic04" else 32
            entries.append((new_typ, png_bytes(sizes[dim])))
            swaps.append(f"{typ.decode()}->{new_typ.decode()}")
        elif typ in dim_of:
            entries.append((typ, png_bytes(sizes[dim_of[typ]])))
        else:
            print(f"  WARNING: unknown chunk {typ!r} copied verbatim")
            entries.append((typ, payload))
    icns_path.write_bytes(build_icns(entries))
    print(f"chatterino.icns: rebuilt, {len(entries)} chunks"
          + (f", legacy swapped: {', '.join(swaps)}" if swaps else ""))

    # 7. icon.svg - own anchor (first saturated fill in document order)
    svg_path = RES / "icon.svg"
    svg = svg_path.read_text()
    fills = re.findall(r'fill="(#[0-9A-Fa-f]{6})"', svg)
    svg_anchor = next(
        (f for f in fills
         if colorsys.rgb_to_hls(*hex_to_rgb01(f))[2] >= 0.15),
        None,
    )
    if svg_anchor is None:
        sys.exit("no saturated fill found in icon.svg")
    _, sa_l, sa_s = colorsys.rgb_to_hls(*hex_to_rgb01(svg_anchor))
    print(f"recolouring svg: anchor {svg_anchor} (S={sa_s:.3f} L={sa_l:.3f})")
    svg_mapper = make_mapper(sa_s, sa_l)

    def repl(m):
        hexv = m.group(1)
        h, l, s = colorsys.rgb_to_hls(*hex_to_rgb01(hexv))
        if s < 0.02:
            return m.group(0)  # keep neutrals
        return 'fill="' + rgb01_to_hex(svg_mapper(h, l, s)) + '"'

    svg2 = re.sub(r'fill="(#[0-9A-Fa-f]{6})"', repl, svg)
    svg_path.write_text(svg2)
    fills_after = sorted(set(re.findall(r'fill="(#[0-9A-Fa-f]{6})"', svg2)))
    print(f"icon.svg fills: {sorted(set(fills))} -> {fills_after}")

    print("done")


if __name__ == "__main__":
    main()
