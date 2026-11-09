#!/usr/bin/env python3
"""Package the existing PPM artwork as a macOS icon without resampling.

Source: ../ppm.ico, the repository's Windows application icon.
The artwork and this script use the repository's BSD 3-Clause license.
Run on macOS: python3 sources/macos/create_icon.py
"""

from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    directory = Path(__file__).resolve().parent
    data = (directory.parent / "ppm.ico").read_bytes()
    reserved, kind, count = struct.unpack_from("<HHH", data)
    if reserved != 0 or kind != 1:
        raise ValueError("Expected a Windows ICO file")

    images = {}
    for index in range(count):
        width, height, _, _, _, _, size, offset = struct.unpack_from(
            "<BBBBHHII", data, 6 + 16 * index
        )
        image = data[offset : offset + size]
        if width != height or not image.startswith(b"\x89PNG\r\n\x1a\n"):
            raise ValueError("Expected square PNG images in ppm.ico")
        images[width or 256] = image

    # Use the source's native 16–256 px artwork, including Retina variants.
    # Larger representations are deliberately not synthesized from small PNGs.
    representations = {
        "icon_16x16.png": 16,
        "icon_16x16@2x.png": 32,
        "icon_32x32.png": 32,
        "icon_32x32@2x.png": 64,
        "icon_128x128.png": 128,
        "icon_128x128@2x.png": 256,
        "icon_256x256.png": 256,
    }
    with tempfile.TemporaryDirectory(prefix="ppm-icon-") as temporary:
        iconset = Path(temporary) / "ppm.iconset"
        iconset.mkdir()
        for name, size in representations.items():
            (iconset / name).write_bytes(images[size])
        subprocess.run(
            ["iconutil", "-c", "icns", str(iconset), "-o", str(directory / "ppm.icns")],
            check=True,
        )


if __name__ == "__main__":
    main()
