# Development

Dustline is written in C++ with [Butano](https://github.com/GValiente/butano) and built with devkitARM. The build pins Butano 21.8.0 and runs the compiler in Docker so contributors do not need a global GBA toolchain.

## Build the ROM on Windows

Install Git and Docker Desktop, switch Docker to Linux containers, and run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The script fetches the pinned Butano source, builds the toolchain image, regenerates derived assets, and writes the ROM to `dist/dustline.gba`.

Generated graphics, audio, and headers should not be edited by hand. Their sources live in `tools/generate_assets.py`, the other generators under `tools/`, the map recipes, and the music project.

## Verify a build

After building the ROM, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1
```

The suite uses headless mGBA and normal joypad input to check generation, driving, collisions, scenes, settings, combat, spawning, and frame budgets. Controller feel and physical hardware behavior still require human playtesting.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/` | Runtime implementation |
| `include/` | Interfaces, tuning, and generated headers |
| `tools/` | Asset generation, tests, and workshop code |
| `maps/` | Map recipes, sources, art profiles, and Tiled worlds |
| `music/` | Adaptive soundtrack project |
| `graphics/` | GBA graphics sources and generated assets |
| `dist/` | Playable ROM and release artifacts |
| `artifacts/` | Emulator captures and test reports |
| `docs/` | Source for this website |

Physics stays separate from presentation and uses fixed-point arithmetic. The simulation advances once per GBA frame, approximately 59.73 times per second. The elevated 2D presentation deliberately avoids Mode 7 and 3D rendering.

## Preview the website

Zensical is pinned in `requirements-docs.txt`. Create a local environment once:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-docs.txt
```

Start a live preview at `http://127.0.0.1:8000`:

```powershell
.\.venv\Scripts\zensical.exe serve
```

Build the same static output used by GitHub Pages:

```powershell
.\.venv\Scripts\zensical.exe build --clean --strict
```

The generated site is written to `site/` and is ignored by Git.

## Publishing

Every push to `main` runs `.github/workflows/docs.yml`. The workflow installs the pinned Zensical version, performs a clean strict build, uploads `site/`, and deploys it through GitHub Pages.

The repository's Pages source is set to **GitHub Actions**. The published site is available at [abhuva.github.io/dustline](https://abhuva.github.io/dustline/).
