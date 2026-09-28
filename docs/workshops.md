# Map and music workshops

Dustline includes browser-based tools for shaping the procedural worlds and adaptive soundtrack. They run locally and write the same source files consumed by the ROM build.

## Map Workshop

Build the ROM once so the workshop has generated terrain art, then start it from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\map-editor.ps1
```

Keep the terminal open and visit [http://127.0.0.1:8765](http://127.0.0.1:8765). After the first workshop build, use the quicker startup when the C++/WebAssembly engine has not changed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\map-editor.ps1 -NoBuild
```

Pass `-Port 9000` to select another port. Maps saved with **In game** enabled are compiled into the ROM during the next game build.

The workshop controls map recipes, art banks, materials, decorations, walls, town sets, roads, encounter placement, and full-world preview exports. Each distinct in-game art profile is packed into a deduplicated tile and palette bank.

## Music Workshop

With the same local server running, open [http://127.0.0.1:8765/music.html](http://127.0.0.1:8765/music.html).

The workshop edits `music/dustline-drive.json`, provides immediate browser playback, and generates the same eight-channel MOD, section table, and reference WAV used by the GBA build.

The soundtrack moves between cruise, fast-driving, nearby-enemy, and combat arrangements at two-bar boundaries. Escalation can happen immediately, while de-escalation waits to prevent rapid changes around speed and enemy-distance thresholds.

Save in the workshop and run `build.ps1` to include the updated soundtrack in the ROM.

