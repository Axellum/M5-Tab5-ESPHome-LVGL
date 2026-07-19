# -*- coding: utf-8 -*-
"""Génère les animations 4:3 Tab5 pour GitHub / Hackster (coupes nettes, sans fondu).

Sorties (docs/images/) :
  - tab5_ui_tour.webp      — recommandé README (léger, 900×675)
  - tab5_ui_tour_hq.webp   — qualité supérieure 1200×900
  - tab5_ui_tour.gif       — fallback universel
  - tab5_ui_tour.mp4       — meilleure compression vidéo
  - tab5_hero_4x3.jpg     — still hero (accueil)
"""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageEnhance
import imageio_ffmpeg

ASSETS = Path(r"C:\Users\axell\.cursor\projects\h-AuxFilsDesIdees\assets")
OUT_DIR = Path(__file__).resolve().parent
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()

HOLD_MS = 1800

SOURCES = [
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_Accueil_meteo-b33b61b2-0885-4269-a771-3e79a10a5501.png",
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_Domo-3e3266d9-dd64-487f-a0ca-c6e62724de02.png",
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_Pots-eb6f7811-bb74-43ae-b619-04db69198dfe.png",
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_Clim-e0ac8928-0cbf-4893-bbc3-2193dc99b955.png",
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_Lumieres-88c9400c-3a58-4d60-865d-09a226ebeed1.png",
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_TV-b5c0dfa7-e0c7-42d5-89b2-f51aef62a9b2.png",
    "c__Users_axell_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_Tab5_Systeme-73e1ada2-e405-4709-b038-499c5d9079d3.png",
]


def crop_to_43(im: Image.Image, size: tuple[int, int]) -> Image.Image:
    im = im.convert("RGB")
    w, h = im.size
    target = 4 / 3
    if w / h > target:
        nw = int(round(h * target))
        left = (w - nw) // 2
        im = im.crop((left, 0, left + nw, h))
    elif w / h < target:
        nh = int(round(w / target))
        top = (h - nh) // 2
        im = im.crop((0, top, w, top + nh))
    return im.resize(size, Image.Resampling.LANCZOS)


def polish(im: Image.Image) -> Image.Image:
    im = ImageEnhance.Contrast(im).enhance(1.06)
    im = ImageEnhance.Color(im).enhance(1.08)
    im = ImageEnhance.Sharpness(im).enhance(1.12)
    return im


def load_slides(size: tuple[int, int]) -> list[Image.Image]:
    out = []
    for name in SOURCES:
        src = ASSETS / name
        if not src.exists():
            raise FileNotFoundError(src)
        out.append(polish(crop_to_43(Image.open(src), size)))
    return out


def save_webp(frames: list[Image.Image], path: Path, quality: int) -> None:
    durs = [HOLD_MS] * len(frames)
    frames[0].save(
        path,
        format="WEBP",
        save_all=True,
        append_images=frames[1:],
        duration=durs,
        loop=0,
        quality=quality,
        method=6,
    )


def expand_for_ffmpeg(slides: list[Image.Image], fps: int) -> list[Image.Image]:
    nrep = max(1, int(round(HOLD_MS / 1000 * fps)))
    out: list[Image.Image] = []
    for s in slides:
        out.extend([s] * nrep)
    return out


def encode_gif(frame_dir: Path, path: Path, w: int, h: int, fps: int) -> None:
    pattern = str(frame_dir / "f%04d.png")
    palette = frame_dir / "palette.png"
    subprocess.run(
        [
            FFMPEG, "-y", "-framerate", str(fps), "-i", pattern,
            "-vf",
            f"fps={fps},scale={w}:{h}:flags=lanczos,palettegen=max_colors=192:stats_mode=diff",
            str(palette),
        ],
        check=True,
        capture_output=True,
    )
    subprocess.run(
        [
            FFMPEG, "-y", "-framerate", str(fps), "-i", pattern, "-i", str(palette),
            "-lavfi",
            f"fps={fps},scale={w}:{h}:flags=lanczos[x];"
            "[x][1:v]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle",
            "-loop", "0",
            str(path),
        ],
        check=True,
        capture_output=True,
    )


def encode_mp4(frame_dir: Path, path: Path, fps: int) -> None:
    pattern = str(frame_dir / "f%04d.png")
    tmp_mp4 = frame_dir / "out.mp4"
    subprocess.run(
        [
            FFMPEG, "-y", "-framerate", str(fps), "-i", pattern,
            "-c:v", "libx264", "-pix_fmt", "yuv420p",
            "-crf", "21", "-preset", "medium",
            "-movflags", "+faststart",
            str(tmp_mp4),
        ],
        check=True,
        capture_output=True,
    )
    shutil.copy2(tmp_mp4, path)


def dump_frames(frames: list[Image.Image], tmp_path: Path) -> None:
    for i, fr in enumerate(frames):
        fr.save(tmp_path / f"f{i:04d}.png")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    slides = load_slides((900, 675))
    save_webp(slides, OUT_DIR / "tab5_ui_tour.webp", quality=72)

    with tempfile.TemporaryDirectory(prefix="tab5_gif_") as tmp:
        tmp_path = Path(tmp)
        dump_frames(expand_for_ffmpeg(slides, fps=10), tmp_path)
        encode_gif(tmp_path, OUT_DIR / "tab5_ui_tour.gif", 900, 675, 10)

    slides_hq = load_slides((1200, 900))
    save_webp(slides_hq, OUT_DIR / "tab5_ui_tour_hq.webp", quality=76)
    slides_hq[0].save(OUT_DIR / "tab5_hero_4x3.jpg", "JPEG", quality=90, optimize=True)

    with tempfile.TemporaryDirectory(prefix="tab5_mp4_") as tmp:
        tmp_path = Path(tmp)
        dump_frames(expand_for_ffmpeg(slides_hq, fps=12), tmp_path)
        encode_mp4(tmp_path, OUT_DIR / "tab5_ui_tour.mp4", fps=12)

    print("Fichiers générés (sans fondu, hold %.1fs) :" % (HOLD_MS / 1000))
    for p in [
        OUT_DIR / "tab5_ui_tour.webp",
        OUT_DIR / "tab5_ui_tour_hq.webp",
        OUT_DIR / "tab5_ui_tour.gif",
        OUT_DIR / "tab5_ui_tour.mp4",
        OUT_DIR / "tab5_hero_4x3.jpg",
    ]:
        print(f"  {p.name}: {p.stat().st_size / 1024 / 1024:.2f} Mo")


if __name__ == "__main__":
    main()
