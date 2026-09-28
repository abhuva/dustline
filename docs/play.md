# Play Dustline

[Download `dustline.gba`](https://github.com/abhuva/Dustline/releases/latest/download/dustline.gba){ .md-button .md-button--primary }

Open the ROM with [mGBA](https://mgba.io/) or RetroArch's **Nintendo - Game Boy Advance (mGBA)** core. Dustline is a complete homebrew ROM: it does not need a base game, patch, or separate GBA BIOS.

!!! note "Prototype status"
    Contracts, credits, inventory, and settings last for the current run only. The game does not have cartridge save persistence yet.

## Controls

| Button | Driving | Menus and towns |
| --- | --- | --- |
| ++a++ | Accelerate | Confirm, interact, enter doors |
| ++b++ | Brake, then reverse | Cancel |
| ++left++ / ++right++ | Steer | Choose or adjust values |
| ++up++ / ++down++ | — | Choose rows; walk in towns |
| ++l++ | Fire the fitted top special | Switch vehicle-lab panel |
| ++r++ | Fire front and side weapons | Switch panel; show weapon info |
| ++select++ | Open the vehicle lab | Return from pause to map selection |
| ++start++ | Pause | Resume |

## First drive

1. Choose a map with Left or Right on the title screen, then press A.
2. Release the throttle before a bend and steer through it.
3. Apply power again as the car points toward the exit.
4. Drive east from the starting point to reach the nearest outpost.
5. Enter the outpost approach area, choose **Yes**, and press A.

Inside town, follow the central street north to the signed garage. The job office is on the western side and the race office is opposite it.

## Vehicle lab

Press Select while driving to pause and open the vehicle lab. Up and Down select a property; Left and Right adjust it. Press A to restore the fitted garage preset. L and R switch between handling and audio controls.

Music starts at 0%, while sound effects start at 100%. Audio controls allow separate volume changes and a master mute for the current session.

## Known limits

- No cartridge saves or persistent progression.
- All six outposts currently share the first town and garage layout.
- No full shop economy, passengers, escort AI, or dialogue trees yet.
- Automated testing uses the mGBA emulator core; physical cartridge behavior still needs verification.

