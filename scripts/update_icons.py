#!/usr/bin/env python3
"""
Update DShare Icons
====================
This script reads the master macOS icon (DShare.icns) and regenerates:
1. Linux deployment icon (PNG 512x512)
2. Windows application icon (ICO multi-size)
3. Windows WiX installer icons (Dialog & Banner)
4. macOS DMG volume icon (icns)
"""

import os
import shutil
from pathlib import Path
from PIL import Image

import sys
import argparse

# Configuration
PROJECT_ROOT = Path(__file__).parent.parent
DEFAULT_SOURCE = PROJECT_ROOT / "src/apps/res/DShare-HID.icns"
ICNS_PATH = PROJECT_ROOT / "src/apps/res/DShare-HID.icns"
LINUX_ICON_PATH = PROJECT_ROOT / "deploy/linux/org.lockekk.dshare-hid.png"
WINDOWS_ICON_PATH = PROJECT_ROOT / "src/apps/res/dshare.ico"
WIX_DIALOG_PATH = PROJECT_ROOT / "deploy/windows/wix-dialog.png"
WIX_BANNER_PATH = PROJECT_ROOT / "deploy/windows/wix-banner.png"
MAC_VOLUME_ICON_PATH = PROJECT_ROOT / "deploy/mac/dmg-volume.icns"

WINDOWS_ICON_SIZES = [(256, 256), (128, 128), (64, 64),
                      (48, 48), (32, 32), (16, 16)]


def main():
    parser = argparse.ArgumentParser(
        description="Update DShare icons from a source image.")
    parser.add_argument(
        "source", nargs="?", help="Path to source image (PNG/ICNS). Defaults to existing DShare.icns")
    args = parser.parse_args()

    source_path = Path(args.source) if args.source else DEFAULT_SOURCE

    if not source_path.exists():
        print(f"Error: Source image not found at {source_path}")
        return 1

    print(f"Reading source image: {source_path}")
    try:
        img = Image.open(source_path)
        img = img.convert("RGBA")

        # 0. Update ICNS (if source is NOT the ICNS itself)
        if args.source and source_path.absolute() != ICNS_PATH.absolute():
            print(f"Updating macOS ICNS: {ICNS_PATH}")
            # Saving as ICNS
            # Pillow saves ICNS by resizing the image to standard sizes.
            # We ensure we have a high enough res.
            img.save(ICNS_PATH, format="ICNS")
            print("  -> Done")

        # 1. Update Linux Icon (512x512 PNG)
        print(f"Updating Linux icon: {LINUX_ICON_PATH}")
        linux_img = img.resize((512, 512), Image.Resampling.LANCZOS)
        linux_img.save(LINUX_ICON_PATH, format="PNG")
        print("  -> Done")

        # 2. Update Windows Icon (ICO)
        print(f"Updating Windows icon: {WINDOWS_ICON_PATH}")
        icon_entries = []
        for size in WINDOWS_ICON_SIZES:
            resized = img.resize(size, Image.Resampling.LANCZOS)
            icon_entries.append(resized)

        icon_entries[0].save(
            WINDOWS_ICON_PATH,
            format="ICO",
            sizes=WINDOWS_ICON_SIZES,
            append_images=icon_entries[1:]
        )
        print("  -> Done")

        # 2.1 Update Windows WiX Icons
        print(f"Updating Windows WiX icons...")

        def create_wix_icon(target_path, width, height, padding_percent=0.1):
            bg = Image.new("RGB", (width, height), "white")
            target_width = int(width * (1 - padding_percent * 2))
            target_height = int(height * (1 - padding_percent * 2))
            ratio = min(target_width / img.width, target_height / img.height)
            new_size = (int(img.width * ratio), int(img.height * ratio))
            resized_img = img.resize(new_size, Image.Resampling.LANCZOS)
            x = (width - new_size[0]) // 2
            y = (height - new_size[1]) // 2
            bg.paste(resized_img, (x, y), resized_img)
            bg.save(target_path)
            print(f"  -> Generated {target_path} ({width}x{height})")

        create_wix_icon(WIX_DIALOG_PATH, 493, 312, padding_percent=0.1)
        create_wix_icon(WIX_BANNER_PATH, 493, 58, padding_percent=0.05)

        # 2.2 Update Mac DMG Volume Icon
        print(f"Updating Mac DMG volume icon: {MAC_VOLUME_ICON_PATH}")
        # If we just generated the ICNS (or source is ICNS), we can copy it.
        # But if source was PNG, we generated ICNS at ICNS_PATH.
        # So ICNS_PATH should be the source of truth for the .icns file now.
        if ICNS_PATH.exists():
            shutil.copy2(ICNS_PATH, MAC_VOLUME_ICON_PATH)
            print("  -> Done")
        else:
            print(
                f"  -> Warning: {ICNS_PATH} not found, cannot update dmg-volume.icns")

        # 2.3 Update Mac DMG Background (Remove baked-in artifacts)
        # Create a plain white background to ensure no old artifacts persist.
        # Dimensions 600x400 match the bounds in generate_ds_store.applescript
        print(
            f"Updating Mac DMG background: {MAC_VOLUME_ICON_PATH.parent / 'dmg-background.tiff'}")
        bg_width, bg_height = 600, 400
        dmg_bg = Image.new("RGB", (bg_width, bg_height), "white")
        dmg_bg.save(MAC_VOLUME_ICON_PATH.parent /
                    "dmg-background.tiff", format="TIFF")
        print("  -> Done")

        # 3. Update Internal SVGs (Embedded PNG in SVG)
        # We need to replace the vector SVGs with our raster icon wrapped in SVG
        # because we don't have a reliable way to vectorize.
        import base64
        from io import BytesIO

        # Helper to create grayscale PNG content (better for solid JPEGs than stencil)
        def create_grayscale_png_b64(source_img):
            # Convert to Grayscale
            # If source has no alpha (JPEG), this creates a B&W image.
            # If source has alpha, we should preserve it.
            if source_img.mode != 'RGBA':
                source_img = source_img.convert('RGBA')

            # Split alpha
            alpha = source_img.split()[3]

            # Convert RGB part to Grayscale
            gray_img = source_img.convert('L')

            # Re-attach alpha (create LA or RGBA with gray channels)
            # Simplest is to create an RGBA image where R=G=B=Gray
            final_img = Image.merge(
                "RGBA", (gray_img, gray_img, gray_img, alpha))

            # Resize to 64x64
            svg_size = 64
            png_buffer = BytesIO()
            final_img.resize((svg_size, svg_size), Image.Resampling.LANCZOS).save(
                png_buffer, format="PNG")
            return base64.b64encode(png_buffer.getvalue()).decode('ascii')

        # Normal colorful SVG content
        svg_size = 64
        png_buffer = BytesIO()
        img.resize((svg_size, svg_size), Image.Resampling.LANCZOS).save(
            png_buffer, format="PNG")
        png_b64_colorful = base64.b64encode(
            png_buffer.getvalue()).decode('ascii')

        # Generate the Grayscale B64 once
        png_b64_gray = create_grayscale_png_b64(img)

        svg_template = '''<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{size}" height="{size}" viewBox="0 0 {size} {size}">
  <image width="{size}" height="{size}" xlink:href="data:image/png;base64,{b64}"/>
</svg>'''

        # Define targets with their specific requirements
        # (path, use_grayscale)
        targets = [
            # Main App Icons (Colorful)
            (PROJECT_ROOT / "src/apps/res/icons/dshare-dark/apps/64/org.lockekk.dshare-hid.svg", False),
            (PROJECT_ROOT / "src/apps/res/icons/dshare-light/apps/64/org.lockekk.dshare-hid.svg", False),

            # Symbolic/Mono Icons
            (PROJECT_ROOT / "src/apps/res/icons/dshare-dark/apps/64/org.lockekk.dshare-hid-symbolic.svg", True),
            (PROJECT_ROOT / "src/apps/res/icons/dshare-light/apps/64/org.lockekk.dshare-hid-symbolic.svg", True),
        ]

        for path, use_grayscale in targets:
            print(f"Generating SVG: {path}")
            path.parent.mkdir(parents=True, exist_ok=True)

            b64_data = png_b64_gray if use_grayscale else png_b64_colorful

            with open(path, "w") as f:
                f.write(svg_template.format(size=64, b64=b64_data))
            print("  -> Done")

        print("\nAll icons updated successfully.")
        if not args.source:
            print("Note: Run with 'python3 scripts/update_icons.py <path_to_new_icon.png>' to update from a new source.")

        return 0

    except Exception as e:
        print(f"Error processing icons: {e}")
        return 1


if __name__ == "__main__":
    exit(main())
