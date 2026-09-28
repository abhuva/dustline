# Dustline

[![Build and publish ROM](https://github.com/abhuva/Dustline/actions/workflows/release.yml/badge.svg?branch=main)](https://github.com/abhuva/Dustline/actions/workflows/release.yml)
[![Build and publish website](https://github.com/abhuva/Dustline/actions/workflows/docs.yml/badge.svg?branch=main)](https://github.com/abhuva/Dustline/actions/workflows/docs.yml)
[![Download latest ROM](https://img.shields.io/badge/download-latest%20ROM-d8a657)](https://github.com/abhuva/Dustline/releases/latest/download/dustline.gba)
[![License: CC BY-SA 4.0](https://img.shields.io/badge/license-CC%20BY--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-sa/4.0/)

An original driving and combat RPG prototype built for the Game Boy Advance.
Dustline combines momentum-heavy arcade handling, controlled slides, procedural
wastelands, roaming enemy drivers, and outposts offering contracts, garages, and
vehicle setups.

![Dustline gameplay](artifacts/demo.gif)

## Play the latest build

Download **[dustline.gba](https://github.com/abhuva/Dustline/releases/latest/download/dustline.gba)**
from the latest release and open it with [mGBA](https://mgba.io/) or RetroArch's
Nintendo - Game Boy Advance (mGBA) core. This is a complete homebrew ROM; no base
game, patch, or GBA BIOS is required.

The current build is an in-development prototype. Contracts and earned credits
last for the current run only; there are no cartridge saves, inventory, shop
economy, or persistent progression yet.

## Controls

| Button | Driving | Menus and towns |
| --- | --- | --- |
| A | Accelerate | Confirm, interact, enter doors |
| B | Brake, then reverse | Cancel |
| Left / Right | Steer | Choose or adjust menu values |
| Up / Down | Unused | Choose menu rows; walk in towns |
| L | Fire fitted top special | Switch vehicle-lab panel |
| R | Fire fitted front and side weapons | Switch vehicle-lab panel; weapon info |
| Select | Open vehicle lab | Return from pause to map selection |
| Start | Pause | Resume |

Try releasing the throttle before a bend, turning through it, and applying power
again on exit. The car retains momentum while coasting, and its heading can differ
from its direction of travel.

Ground now has distinct handling character: loose material develops a gentle
wander and longer slides, gravel chatters, hardpan produces a light rumble, and
roads remain stable. The HUD label follows the same road-aware material sample as
the physics.

Press Select while driving to open the Handling Lab. It exposes acceleration,
maximum speed, grip, steering, neutral coast drag, brake force and mass over deliberately broad test ranges;
Up/Down chooses a property, Left/Right adjusts it, and A restores the fitted
garage preset. L/R switches between the Handling and Audio Control panels. Audio
Control independently adjusts music and sound-effect volume in 10% steps and
provides a master mute; these choices persist for the current session. Music
starts at 0%, while sound effects start at 100%.

## What's in the prototype

- A native GBA ROM written in C++ with Butano and devkitARM.
- Seven fixed-seed 8192 x 8192 procedural maps selected from the title screen.
- Fixed elevated 2D presentation with momentum, grip-limited sliding, braking,
  reverse, terrain collisions, and three distinct vehicle setups.
- Streaming terrain, a fixed-2x minimap, outposts, walkable towns, and a garage.
- Signed town services with proximity-based, animated button prompts.
- Deterministic courier and marked-raider contracts from town dispatch boards.
- A separate race office with hub-to-outpost road events and generated open or
  closed wilderness courses, sequential gates, scoring, payouts and course-abort rules.
- Pooled enemy drivers and garage-fitted front, side, missile, and trap weapons.
- Player health, a rechargeable shield, battery energy, synthesized effects, an adaptive
  eight-channel tracker soundtrack, and original pixel art.
- Local browser-based map and music workshops shared with the ROM generators.

For the complete gameplay notes, current limitations, architecture, map-workshop
guide, and verification details, see [readme.txt](readme.txt).

## Build locally

On Windows, install Git and Docker Desktop using Linux containers, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The script fetches Butano 21.8.0 at its pinned commit, builds the pinned devkitARM
container, generates all derived assets, and writes the playable ROM to
`dist/dustline.gba`. Generated graphics, audio, and headers should not be edited
by hand; their source is `tools/generate_assets.py` and the other generators in
`tools/`.

To run the emulator-driven verification suite after building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1
```

The automated checks use headless mGBA and normal GBA joypad input. They verify
generation, driving, collisions, scenes, settings, combat, spawning, and frame
budgets; they are not a substitute for subjective controller playtesting.

## Start the development tool

Dustline includes a local browser-based Map Workshop. It requires Docker Desktop
(using Linux containers) and Python 3. From the repository root, run the game
build once so the workshop has the generated terrain art:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

Then build and start the workshop:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\map-editor.ps1
```

Keep that terminal open and visit **[http://127.0.0.1:8765](http://127.0.0.1:8765)**.
Press `Ctrl+C` in the terminal to stop it. After the first workshop build, use
the faster startup command when its C++/WebAssembly engine has not changed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\map-editor.ps1 -NoBuild
```

Use `-Port 9000` to choose another local port. Changes saved with **In game**
enabled are compiled into the ROM the next time `build.ps1` runs. The workshop
runs entirely on `127.0.0.1`; it does not require an external account or service.

Open **[http://127.0.0.1:8765/music.html](http://127.0.0.1:8765/music.html)**
for the Music Workshop. It edits the shared `music/dustline-drive.json` source,
offers immediate browser playback, and generates the same eight-channel MOD,
section table, and reference WAV used by the GBA build. Music switches among
cruise, fast-driving, nearby-enemy, and combat arrangements at two-bar boundaries.
The current *Dustline Horizon* study keeps one melodic signal across those states:
soft-attack pad and desert-air loops carry the quiet sections, while bass,
percussion, countermelody, and glass chimes enter as danger rises.
Its warm instrument bank uses seamless rounded bass curves, a long detuned chorus
pad, circularly filtered wind noise, and separately decaying lead/chime partials
instead of raw saw and pulse oscillators.
Save in the workshop, then run `build.ps1` to include the new version in the ROM.

Use **Art bank** to choose each map's four ground assets and material IDs, four
foreground decorations, wall set, and town set. The build packs one deduplicated
tile/palette bank for every distinct in-game profile; only the selected map's
bank is loaded into GBA VRAM. Save the map, rebuild, and reload the workshop to
see source-art changes or a newly packed profile in the game-texture preview.

## Continuous delivery

Every push to `main` runs the reproducible container build in GitHub Actions and
replaces the rolling **[Latest playable build](https://github.com/abhuva/Dustline/releases/latest)**.
The release contains the raw ROM, its SHA-256 checksum, and a ZIP with the ROM,
play notes, and third-party notices.

The same push builds the [Dustline website and documentation](https://abhuva.github.io/dustline/)
with Zensical and publishes it through GitHub Pages.

## Project direction

The immediate priority is satisfying driving: readable grip, throttle release,
controlled slides, and meaningful vehicle setups. RPG progression will grow on
top of that foundation after more playtesting. The game takes inspiration from
the feel of classic GBA vehicle adventures, but uses original code, art, audio,
names, physics, and world design.

## Credits and licensing

Dustline uses [Butano](https://github.com/GValiente/butano) and devkitARM. The
full upstream notice set ships in `dist/licenses/` and in each packaged release.

Except for separately identified third-party material, Dustline's game-specific
code, artwork, audio, documentation, and compiled ROM are licensed under the
[Creative Commons Attribution-ShareAlike 4.0 International License](LICENSE).
Credit **Dustline by Marc Bielert**, link to this repository and the license, and
indicate whether you made changes. Third-party components remain under their
respective licenses.
