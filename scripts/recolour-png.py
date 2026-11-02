#!/usr/bin/env python3
# satorino: recolour flat PNG logos to the fork colour (#cba6f7), using the
# exact same method as scripts/recolour-icon.py (anchor = the PNG's dominant
# saturated colour, which maps exactly to the target; neutrals and alpha are
# untouched). The files are rewritten in place.
#
# Used for resources/settings/leafyrino.png, the settings-sidebar tab logo
# (same artwork as the app icon). Re-run after an upstream sync replaces it:
#
#     python3 scripts/recolour-png.py resources/settings/leafyrino.png
#
# Never use this on emotes, badges or avatars.

import importlib.util
import pathlib
import sys

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location(
    "recolour_icon", HERE / "recolour-icon.py"
)
ri = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ri)


def main(argv):
    if not argv:
        sys.exit("usage: recolour-png.py FILE.png [FILE.png ...]")
    for name in argv:
        path = pathlib.Path(name)
        print(path)
        img = Image.open(path)
        anchor_s, anchor_l = ri.find_anchor(img)
        out = ri.recolour_image(img, ri.make_mapper(anchor_s, anchor_l))
        out.save(path, optimize=True)


if __name__ == "__main__":
    main(sys.argv[1:])
