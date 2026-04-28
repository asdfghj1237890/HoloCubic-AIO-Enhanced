"""Generate the HoloCubic AIO Tool icon.

Design: isometric cube wireframe with cyan holographic glow on dark
background, evoking the HoloCubic device's holographic display.
Run: ``uv run python scripts/make_logo.py``
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

# Output settings — generate at 4x supersample then downscale for crisp anti-aliasing
TARGET_SIZES: tuple[int, ...] = (16, 32, 48, 64, 128, 256)
SUPERSAMPLE = 4
W = 256 * SUPERSAMPLE  # render at 1024x1024
H = W

# Color palette (matches CTk dark theme)
BG_TOP = (15, 23, 42)  # slate-900
BG_BOTTOM = (2, 6, 23)  # near-black
CYAN_BRIGHT = (0, 240, 255)  # primary glow
CYAN_DEEP = (10, 130, 200)  # secondary edge
BLUE_ACCENT = (31, 106, 165)  # CTk theme blue
GRID_DIM = (30, 50, 80, 80)  # faint grid lines


def _radial_gradient_bg() -> Image.Image:
    """Dark radial gradient background — darker at edges, slightly lit centre."""
    img = Image.new("RGBA", (W, H), BG_BOTTOM + (255,))
    draw = ImageDraw.Draw(img)
    cx, cy = W // 2, H // 2
    max_r = int(math.hypot(cx, cy))
    for r in range(max_r, 0, -2):
        t = r / max_r  # 0 (centre) -> 1 (edge)
        col = tuple(int(BG_TOP[i] * (1 - t) + BG_BOTTOM[i] * t) for i in range(3))
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=col + (255,))
    return img


def _grid_overlay() -> Image.Image:
    """Faint diagonal grid (cyberpunk circuit hint)."""
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    spacing = 32 * SUPERSAMPLE
    line_w = 1 * SUPERSAMPLE
    for x in range(-H, W + H, spacing):
        draw.line((x, 0, x + H, H), fill=GRID_DIM, width=line_w)
        draw.line((x, H, x + H, 0), fill=GRID_DIM, width=line_w)
    return img


def _isometric_cube_vertices(cx: int, cy: int, size: int) -> dict[str, tuple[int, int]]:
    """Return 8 vertices of an isometric cube (front-top-left, etc.)."""
    # Isometric projection: x' = x - z, y' = (x + z)/2 - y, scaled.
    # Use 30-degree rotation of square => width:height ratio 2:1.
    s = size
    half = s // 2
    quarter = s // 4
    # Centre the cube on (cx, cy)
    return {
        # Top face (rhombus): top, right, bottom, left
        "TT": (cx, cy - half),
        "TR": (cx + s, cy - quarter),
        "TB": (cx, cy),
        "TL": (cx - s, cy - quarter),
        # Bottom face: corresponding shifted down
        "BT": (cx, cy - half + s),
        "BR": (cx + s, cy - quarter + s),
        "BB": (cx, cy + s),
        "BL": (cx - s, cy - quarter + s),
    }


def _draw_glow_line(
    img: Image.Image,
    p1: tuple[int, int],
    p2: tuple[int, int],
    color: tuple[int, int, int],
    width: int,
) -> None:
    """Draw a line with an additional thicker translucent layer for glow."""
    draw = ImageDraw.Draw(img)
    # Outer glow (wider, dimmer)
    draw.line((p1, p2), fill=color + (60,), width=width * 4)
    # Mid glow
    draw.line((p1, p2), fill=color + (140,), width=width * 2)
    # Core
    draw.line((p1, p2), fill=color + (255,), width=width)


def _draw_cube(img: Image.Image, scale: float = 0.55) -> None:
    """Draw the isometric holo cube on top of the image."""
    cube_size = int(W * scale * 0.5)
    cx, cy = W // 2, int(H * 0.52)
    v = _isometric_cube_vertices(cx, cy, cube_size)

    line_w = 4 * SUPERSAMPLE

    # Back edges (deeper colour, drawn first so front edges occlude)
    back_edges = [
        (v["TT"], v["BT"]),  # back-top vertical
        (v["TT"], v["TR"]),  # top-back-right edge
        (v["TT"], v["TL"]),  # top-back-left edge
    ]
    for p1, p2 in back_edges:
        _draw_glow_line(img, p1, p2, CYAN_DEEP, line_w)

    # Front edges (bright cyan)
    front_edges = [
        # Top face front edges
        (v["TR"], v["TB"]),
        (v["TL"], v["TB"]),
        # Verticals
        (v["TR"], v["BR"]),
        (v["TL"], v["BL"]),
        (v["TB"], v["BB"]),
        # Bottom face front edges
        (v["BR"], v["BB"]),
        (v["BL"], v["BB"]),
    ]
    for p1, p2 in front_edges:
        _draw_glow_line(img, p1, p2, CYAN_BRIGHT, line_w)

    # Vertices as small bright dots
    dot_r = 6 * SUPERSAMPLE
    draw = ImageDraw.Draw(img)
    for x, y in v.values():
        # Halo
        draw.ellipse(
            (x - dot_r * 2, y - dot_r * 2, x + dot_r * 2, y + dot_r * 2), fill=CYAN_BRIGHT + (80,)
        )
        # Core
        draw.ellipse((x - dot_r, y - dot_r, x + dot_r, y + dot_r), fill=(255, 255, 255, 255))


def _draw_holo_ring(img: Image.Image) -> None:
    """A faint outer ring (the hologram beam projector)."""
    draw = ImageDraw.Draw(img)
    cx, cy = W // 2, H // 2
    # Outer ring
    r_out = int(W * 0.46)
    r_in = int(W * 0.43)
    for r in range(r_in, r_out, 2):
        alpha = int(60 * (1 - (r - r_in) / max(1, r_out - r_in)))
        draw.ellipse((cx - r, cy - r, cx + r, cy + r), outline=BLUE_ACCENT + (alpha,), width=2)
    # Stronger arc segments (4 quadrant marks for tech feel)
    arc_r = int(W * 0.44)
    for start, extent in ((0, 30), (90, 30), (180, 30), (270, 30)):
        draw.arc(
            (cx - arc_r, cy - arc_r, cx + arc_r, cy + arc_r),
            start=start,
            end=start + extent,
            fill=CYAN_BRIGHT + (200,),
            width=4 * SUPERSAMPLE,
        )


def _draw_text_aio(img: Image.Image) -> None:
    """Draw small 'AIO' text at the bottom — using simple rectangles to avoid
    font dependency."""
    draw = ImageDraw.Draw(img)
    # Just a thin underline mark at the bottom — keep the icon clean
    cx = W // 2
    y = int(H * 0.88)
    bar_w = int(W * 0.18)
    bar_h = 6 * SUPERSAMPLE
    draw.rounded_rectangle(
        (cx - bar_w // 2, y, cx + bar_w // 2, y + bar_h),
        radius=bar_h // 2,
        fill=CYAN_BRIGHT + (220,),
    )


def render() -> Image.Image:
    """Render the full logo at supersampled resolution."""
    bg = _radial_gradient_bg()
    grid = _grid_overlay()
    bg = Image.alpha_composite(bg, grid)

    # Add a glow layer beneath the cube for ambient lighting
    glow_layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    _draw_holo_ring(glow_layer)
    _draw_cube(glow_layer)
    _draw_text_aio(glow_layer)

    # Apply soft blur to glow layer for halo, then re-overlay sharp version
    halo = glow_layer.filter(ImageFilter.GaussianBlur(radius=8 * SUPERSAMPLE))
    bg = Image.alpha_composite(bg, halo)
    bg = Image.alpha_composite(bg, glow_layer)

    return bg


def save_ico(img: Image.Image, out_path: Path) -> None:
    """Write multi-resolution .ico (16/32/48/64/128/256) — Windows uses these."""
    # Downsample the rendered supersampled image to each target size
    versions = [img.resize((s, s), Image.Resampling.LANCZOS) for s in TARGET_SIZES]
    # PIL's ICO writer accepts sizes via the sizes= argument
    versions[-1].save(
        out_path,
        format="ICO",
        sizes=[(s, s) for s in TARGET_SIZES],
        append_images=versions[:-1],
    )


def save_png_preview(img: Image.Image, out_path: Path) -> None:
    """256x256 PNG preview for README / GitHub."""
    img.resize((256, 256), Image.Resampling.LANCZOS).save(out_path, format="PNG")


def main() -> None:
    here = Path(__file__).resolve().parent.parent  # AIO_Tool/
    out_dir = here / "image"
    out_dir.mkdir(parents=True, exist_ok=True)

    img = render()
    ico_path = out_dir / "holo_256.ico"
    png_path = out_dir / "holo_256.png"
    save_ico(img, ico_path)
    save_png_preview(img, png_path)
    print(f"wrote {ico_path}  ({ico_path.stat().st_size} bytes, {len(TARGET_SIZES)} sizes)")
    print(f"wrote {png_path}  ({png_path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
