DUSTLINE - PROCEDURAL WASTELANDS
Playable Game Boy Advance driving/combat prototype / version 0.16

PLAY
----
Open dist/dustline.gba in RetroArch using the Nintendo - Game Boy Advance
(mGBA) core. Use Load Content to select the file directly; a database scan
may not recognize an original homebrew game. No base game or ROM patch is
needed. A separate GBA BIOS is optional with mGBA.

Choose START NEW GAME and press A to begin at the authored world start with a
fresh car, inventory and progression state. CONTINUE GAME appears only when a
valid cartridge save exists and restores that save's region and position. The
ROM currently includes the three enabled 8192 x 8192 procedural regions;
disabled catalog drafts are not compiled and regions are reached through
reciprocal cardinal exits generated from the World Map.
There is no runtime random-map choice. Recipes can
use cellular automata, masks, smoothing and largest-component flood fill so
disconnected floor is filled and all six outposts are reachable.
The loading screen shows aggregate progress through the selected map's world,
material, spawn and decoration node graphs, followed by encounter and scene setup.
Select opens a paused four-page field menu without resetting the car. Its first
page is a complete 128 x 128 region map with gold towns, green exits, the red player and the active
objective; the remaining pages contain driving setup, audio and save data. While driving, R fires the
fitted front and side weapons together and L fires the fitted top special.
Weapon fitting is changed only at a garage; there is no loadout switching while driving.
Start pauses and shows the active contract, reward, completed-job count and
session credits.
Music starts at 0% volume and can be enabled from the field menu's MUSIC
CONTROL panel. It is an original eight-channel generated tracker module and moves among
CRUISE, FAST DRIVE, ENEMY NEARBY and COMBAT arrangements at two-bar boundaries.
Escalation is armed immediately; de-escalation waits three seconds to avoid rapid
flapping near speed and enemy-distance thresholds. Pause, settings and town scenes
keep the current soundtrack at a lower volume. Returning to the title stops it.
Drive east from spawn to try the nearby first outpost (256px away).
Entering the circular approach area centred on an outpost stops the car and asks
whether to enter; it works from every side. Declining suppresses the question
only until the car leaves that circle, so returning triggers it immediately. Use a direction
to select Yes/No, then A to confirm; B cancels. No is selected initially.
Yes unloads the overworld graphics and opens a walkable, lively wasteland town.
Follow the central street to the signed GARAGE at the north edge. Enter its
approach area and press A when the circled A prompt appears. Inside, enter the
rectangular area along the mechanic counter and press A when its prompt appears
to fit a vehicle setup or open the parts shop. The shop packs its fixed,
town-specific stock into a grid of up to 3 x 3 icons:
use the D-pad to select an item, A to buy it for gold plus scrap, R for details,
and B to return to setup. Bought items disappear from every town that stocked
them without being replaced; an empty shop clearly reports SOLD OUT. Bought
upgrades activate immediately and bought weapons become available at the parked car. The parked car on the left is
solid; stand in the marked area immediately to its right and press A to enter the
separate WEAPON FITTING screen. Its larger vehicle preview shows the fitted
attachments. Left/Right chooses FRONT, SIDE or TOP, A opens that mount's
compatible inventory, the D-pad chooses an item, A fits it and B backs out or
closes the screen. R opens data for the highlighted weapon. Returning reloads
the garage at the same position. The garage exit has the same prompt and proximity rule;
none of these interactions require a facing direction. Leave through the garage
door, then use the settlement's south gate, marked with a floating A prompt, to return to the
exact saved coordinates, stopped and facing 180 degrees away from the outpost, on the same map. Leave the entrance vicinity
before its prompt can reopen.
The settlement marker remains visible above the bottom-aligned entry question.
All six outposts currently share this first town and garage layout. There are no
additional interiors or dialogue trees yet.

CONNECTED REGIONS
-----------------
The enabled procedural regions form an authored cardinal world graph. Every map
has fixed north, east, south and west ports, and one saved connection joins two
complementary sides reciprocally. Driving into a connected edge trigger stops the car
and asks whether to travel to the connected region. No is selected initially;
use any D-pad direction to choose Yes, A to confirm, or B to decline. Declining
suppresses that exit only until the car leaves its rectangle, so returning asks
again immediately. The same shared connection always provides the return trip.

Arrival is derived from the destination side at a safe fixed inset and faces
inward; normal travel never falls back to the New Game spawn. Health, shield,
battery energy, fitted weapons, tuning,
inventory, credits and an active contract travel with the player. The destination
region's terrain is regenerated from its fixed recipe and its enemies, pickups,
projectiles and other local simulation start fresh; regions do not retain local
world state yet.

CONTRACTS
---------
Each outpost has a signed JOB office on the western building. Enter its approach
area and press A when the circled A prompt appears; facing direction does not matter. Use
Left/Right to choose COURIER or HUNT, A to accept, and B to close the board. Only
one contract can be active at a time.

Courier contracts choose a deterministic outpost in another reachable region.
Enter that outpost to complete the delivery. Hunt contracts choose one specific
encounter anchor in another reachable region; destroy that enemy to complete the job. A
white/gold/red marker on the minimap shows the target or clamps to its edge to
give a direction when the target is farther away. Outside the objective region
it instead points to the next cardinal exit on the shortest route. Contract
rewards scale with the number of region hops and become
session credits immediately on completion. Return to any dispatch board and
press A on the completion notice to request another job.

Offers derive from the selected map seed, origin outpost and contract serial, so
the same run state produces the same offer. Contract state survives town visits,
pausing, settings and revival. START NEW GAME clears the active in-memory run;
returning to the title leaves the cartridge save available for CONTINUE GAME.
There is no contract-abort option, passenger simulation or escort
AI yet.

RACES
-----
The eastern building opposite the JOB office is signed RACE. Enter its approach
area and press A to open the race board, then use Left/Right to choose TOWN RACE
or WILD RACE. Racing has separate state from contracts: an accepted courier or
hunt remains intact, but its marker and progress are suspended during a race.

Town races follow the generated road tree. The central outpost selects one
outer destination; an outer outpost always races back to the central one. The
timer starts after leaving town and a three-second countdown. Pass the numbered
gates in order and cross the last gate beside the destination outpost. Score is
normalized against the route's generated par time. Scores below 700 pay nothing;
700, 900 and 1100 points reach the bronze, silver and gold payment tiers.

Wild races generate either an open point-to-point course or a closed loop on
clear reachable ground outside every town's exclusion area. Accepting teleports
the car to the start. Finishing, wrecking or aborting returns the player beside
the originating race office. The active gate is a bright ground ring bracketed
by two small flags aligned across the route; crowded flags move inward so they
do not sit inside walls. A crossed gate and its flags linger in the world for
about one second, while the radar immediately advances to the next gate and
clamps it to the edge when necessary. Wild courses use sixteen gates for
closer guidance. Staying away from the hidden course corridor for about eight
seconds, or making no gate progress for about forty-five seconds, ends the event
automatically. Pause and press B for a manual abort. Only a completed race
meeting a score tier awards session credits.

BASIC COMBAT TEST
Scout buggies, armed raiders and heavy trucks spawn throughout procedural maps.
Each reachable 512px sector gets an encounter anchor where clearance permits,
plus two nearby starter opponents. The active exported recipe has 163 anchors; other seeds
vary. A shared pool limits the entire simulation to FIVE active enemies.
Anchors activate within 560px (about 2.3 screen widths); cars despawn beyond
800px from the player. This wider removal radius avoids boundary flicker.
New cars spawn outside the viewport, with clearance from existing cars. Initial
scene loading may place the starter pair in view. Nearby eligible points get
priority when a slot is free; the pool never grows to accommodate more points.
Each authored population profile controls its enemy body, health behavior,
respawn interval and loot table. The Wasteland example bands northern scouts,
central raiders and southern heavies. A visible anchor waits until it is
off-screen. Despawned survivors retain their remaining HP, but restart at their
anchor when reactivated. Anchors cannot produce duplicates while their car lives.
Each car has three fixed garage mounts: FRONT accepts the standard gun or long-range
sniper, SIDE accepts the paired broadside guns or twin forward shooter, and TOP
accepts a homing missile, trap or passive radio-signal receiver. Every slot can also
be left EMPTY. Hold R to fire the fitted front and side mounts together; weapons
do not wait for an AI range decision. L independently operates the fitted TOP
special. The radio uses no energy and makes L a no-op; while fitted, it shows one,
two or three world-relative chevrons near the car pointing toward the nearest
active signal in range. A new game owns and fits only the standard front gun; all
other equipment must be bought at the garage shop before it appears in fitting. There is no weapon
switching or weapon icon during map play, and no full-width driving status bar.
The default forward gun fires about six shots per second with 120px muzzle
travel; side guns fire both +/-90-degree directions every 16 driving frames.
Each bullet removes one of the enemy's three HP pips.
The long-range sniper travels 520px, deals four HP and costs 5 energy. Its
75-frame cooldown makes it a deliberate single shot rather than a rapid-fire gun.
The SIDE-slot forward shooter emits two parallel forward bullets every 18 frames,
costing 3 energy per volley.
The chainsaw is reserved for a later dedicated system. It cannot be fitted or
activated through the current three-slot garage and driving controls.
Homing missiles launch 18px ahead at 1.25px/frame, coast for eight ticks, then
acquire the closest living enemy. They accelerate smoothly to 3px/frame over
roughly their first second and refresh direction every six ticks. They deal
three HP and expire after 150 ticks. At most two exist; shots are spaced 36 ticks
apart. Targets use encounter identity and are reacquired after loss.
Traps drop 22px behind, arm after 18 ticks, and stay stationary for up to 900
ticks. Enemy contact detonates a 32px-radius, three-HP blast. The owner's car
does not trigger it. Six traps maximum; one drop per 45 ticks. A full pool
refuses another drop. Traps flash when armed and show a blast when triggered.
Each enabled procedural region maintains three to five randomly placed radio
discoveries during a run. The passive receiver automatically tracks the nearest
one within its hard 1024px range; it uses no energy and requires no held button.
Salvage barrels remain visible at close range even without a receiver. One player
bullet destroys a barrel and immediately awards 12 scrap, then schedules a new
random barrel after roughly three to five minutes of driving. Map travel retains
that run's pools and cooldowns to prevent region-transition refills. Normal save/load starts
fresh activity pools while preserving the fitted receiver; emulator save states
preserve the exact live pools and timers.
Weapons respect walls/buildings and each weapon retains its own cooldown.
Deployed attacks keep working. Pause and settings freeze all simulation. Town
entry clears deployed attacks while preserving the fitted mounts and encounter
damage. A new run starts with only the standard gun and empty attack pools.
The standard battery starts with 100 energy, shown by the amber segmented arc
along the bottom third of the minimap. It empties symmetrically from both ends
toward its bottom centre. The gun is free; broadside guns cost 2 per volley, the
forward shooter costs 3, the sniper costs 5, missiles cost 8 and traps cost 6. Driving
and the basic gun always remain available at zero energy.
The player starts with 100 health and a 20-point rechargeable shield. Enemy
bullets deal one point, consuming shield before health. After three seconds
without damage, shield returns at four points per second, consuming one energy
per restored shield point; recharge stops when the battery is empty. Entering an
outpost fully restores health, shield and energy. At zero health, A revives in
place with full health and shield, preserves the remaining energy, clears
deployed projectiles and grants two seconds of protection.
The compact minimap is framed by segmented vitality arcs: red health on the
left and blue shield on the right, both filling from bottom to top. Exact values
remain available on the pause screen.
Ramming changes motion, not health, for now.
Cars have heading-aligned 22x14px rectangular contact bodies. Impacts transfer
velocity according to relative closing speed and mass, with mild bounce and
tangential friction. Light cars are shoved more than heavy ones; engine speed
limits do not instantly erase an impact's extra speed. Three bounded contact
passes separate groups of cars, checking terrain before every correction.
Terrain driving still uses the established forgiving circular footprint. This
is arcade contact response, not a full chassis/inertia/angular-impact model.
Bullets cannot pass through walls/buildings.
Enemy fire ignores other enemies. Destroyed cars can independently leave physical
scrap or energy-cell pickups according to their authored regional profile.
Pickups pull toward the car at close range; the purchased salvage magnet doubles
that range.
Weapons use no ammunition; powered weapons and shield recharge draw from energy.

Enemies detect/pursue within 480px (two screen widths). Their gun now matches
the player's: 120px muzzle range, 6px/frame bullets, one shot per ten frames.
They only fire with the player inside a roughly +/-9-degree forward cone,
within gun reach and with a clear terrain line of sight. There is no auto-aim.
They use the same driving physics with profile-selected vehicles: light 750kg
RAIDERs in the center, faster scouts in the north, and 2200kg heavies in the
deep south. Normal AI cruising targets 1.4px/frame. Scouts do not carry guns;
the other profiles use the normal forward weapon. Sensors check predicted positions of the player
and other live cars as well as terrain. Near the player they make short flanking
legs and repeated attack runs instead of parking; outside pursuit range they
patrol near their original positions. Rear sensing protects reverse recoveries.
This is local obstacle avoidance, not maze-wide pathfinding: awkward corners
can still confuse them. Brief braking stops and occasional contacts are normal.

Select settings, Start pause, and town prompts freeze combat. Entering town
unloads its sprites and clears bullets in flight, but preserves enemy HP/deaths
and positions, including anchor ownership and cooldowns. Cooldowns only advance
while driving, not in menus or town. Returning does not reset encounters; starting
a new map resets all of them. No encounter state is saved across a reboot.
Driving scenes have no top status strip; speed, detected ground, setup names,
seed, timers and lap counts are not displayed while driving. The freed rows show
terrain. The wasteland uses
a static 64x64 source image clipped by a circular mask, with transparent corners
and a thin rim. Each source pixel is one logical cell. Walls are
bright, floor dark, roads grey and towns gold dots. The yellow player marker
stays centered in the fixed 2x view; the image follows the player. Areas beyond
the map edges appear white like walls. Town dots outside the circle are clipped.
There is no minimap zoom setting or driving zoom input. The source uses 4 KiB
and is built once per scene. The crop updates only when the player
crosses a displayed pixel. Road width and ground materials do not change this
simplified image. Small red dots track living active enemies every frame; dots
outside the circle, destroyed enemies and inactive spawn points are hidden.
During a race only the current checkpoint appears on the minimap.

Select opens the paused field menu. L/R cycles its MAP, DRIVE, MUSIC and
SAVE pages; the highlighted square in the left icon rail shows the active page.
MAP displays the complete 128 x 128 region, all towns, the live player position
and the active race checkpoint or contract destination when one exists. It opens
on CURRENT POSITION; Left/Right cycles the player, every named town and every
connected exit. The selected location receives a large bright marker and its name is
shown along the bottom. Connected exits read ROAD TO followed by their destination.
In DRIVE, Up/Down chooses ACC, SPEED, GRIP,
STEER, COAST, BRAKE, MASS, BAT or CAR. COAST is the base per-frame slowdown while
neither A nor B is held; BRAKE is the forward-speed reduction applied by B.
The existing terrain resistance is added on top, so loose ground still scrubs
more speed than a road at the same COAST value.
On CAR, Left/Right selects ROADSTER, SAND BUGGY, OLD CAR, TRUCK or PICKUP and A
restores ROADSTER. The body choice is visual only; the fitted/tuned setup still
controls physics. BAT selects COMPACT (70 energy, -100 kg), STANDARD (100 energy,
no mass change) or LARGE (150 energy, +200 kg); changing capacity never grants
free energy and reducing it clamps the stored charge. On the other rows,
Left/Right changes the value and A restores the fitted garage preset. Weapon
fitting is deliberately absent here and belongs to the town garage. In MUSIC,
Up/Down chooses music volume, sound-effect volume or master mute.
Left/Right changes either volume in 10% steps; A restores a volume to 100% or
toggles mute. SAVE DATA provides SAVE GAME, LOAD GAME and ERASE SAVE; erase
requires a second A press. Loading always returns to the overworld, including
when the save was made from town. A successful load briefly names the restored
map and says whether the exact saved position was used. Recipe/world-graph edits
do not discard that position: if it is still driveable it is restored exactly;
if changed collision covers it, the nearest safe floor is used and reported as
POSITION MOVED. B, Select or Start returns. Body, tuning,
fitted weapons and audio choices persist across pause, map and town scene changes
for the session and are included in a manual save. Available handling ranges are deliberately wide:
acceleration 0.000-0.500, speed 0.25-12.00, grip 0.000-1.000, steering 0.00-12.00,
coast drag 0.000-0.050, brake force 0.00-0.30 and mass 100-10000 kg.

CONTROLS (GBA BUTTONS, AS MAPPED IN YOUR EMULATOR)
------------------------------------------------
Title Up/Down    Choose Start New Game or Continue Game (with a valid save)
Title A          Confirm the highlighted option
A               Accelerate
B               Brake; keep holding to reverse once stopped
Up / Down       No driving function
Left / Right    Steer relative to the car (steering reverses while backing up)
Town D-pad      Walk in four directions
Town A          Use any circled-A proximity area; facing does not matter
Garage Up/Down Switch mechanic setup/shop pages
Shop D-pad      Select an available town-stocked upgrade or weapon
Shop A / B      Buy highlighted item / return to setup
Shop R          Show highlighted item details
Fitting Left/Right Choose the FRONT, SIDE or TOP mount
Fitting D-pad   Choose a compatible weapon after opening a mount
Fitting A / B   Open or fit / cancel or close the fitting screen
Fitting R       Show data for the highlighted fitted or inventory weapon
R in wasteland  Fire the fitted FRONT and SIDE mounts together
L in wasteland  Fire the fitted TOP special weapon
Select          Open field menu while driving or in town (pauses driving)
L / R           In field menu: cycle Map / Drive / Music / Save page
Up / Down       In Drive/Music/Save: choose a property or action row
Left / Right    In Map: cycle locations; elsewhere edit the selected value
A               Restore/edit the selected row or perform the selected save action
B/Select/Start  In field menu: apply and return
Start           Pause / controls screen; press again to resume
Pause B         Abort an active race (wild races return to their race office)
Select in pause Return to map selection

Reverse uses B, after the car has stopped.

PROGRESSION
-----------
Town Garages sell deterministic regional selections from SALVAGE MAGNET, TUNED
INJECTOR, REINFORCED PLATING, TWIN SIDE GUNS, LONG SNIPER, FRONT SHOOTER, SEEKER
MISSILE, REAR TRAP and SIGNAL RADIO. Wasteland towns emphasize early essentials,
Two Rules adds missile and trap choices, and Twin Cities carries the later-tier
selection. Individual towns within a region can stock different subsets, but
their base inventories never rotate or scale with the player. A purchase unlocks
the item immediately and removes it globally; there are no blueprints or
3D-printing step. Ownership uses stable save IDs, and ownership, resources,
fitted gear and installed upgrades are included in normal saves. Starting a new
game clears purchases and returns to the standard front gun.
Authored enemy populations can also drop energy cells, letting a successful fight
extend an expedition. Outposts restore the battery, while the free gun and driving
prevent an empty-energy dead end.
Front and side weapons fire as a group, with the fitted top mount on L.
Equipment remains independently installable in the garage fitting screen. A future fire-control upgrade
could suppress wasteful out-of-range shots, but the baseline deliberately fires
without target-selection AI or its per-frame decision cost.

TRY THIS FIRST
--------------
Build speed on the straight, release A before a bend, steer into it, and
apply throttle again as you come out. You retain momentum while coasting.
There is no tap-frequency bonus: deliberate throttle timing is the goal.
The car has separate heading and velocity, so its nose can point slightly
away from its direction of travel. Watch the skid marks during a fast turn.
To restart a run, press Start, then Select to return to the title and choose
START NEW GAME. Its saved seed reproduces the same layout; there is no
driving-position reset key.

Enter a wasteland town, walk into the garage and speak to the mechanic to compare:
  GRIP   - 950kg; forgiving cornering grip, moderate power.
  RALLY  - 1100kg; default, more power and speed, looser cornering.
  HEAVY  - 2200kg; slower acceleration/steering, greater resistance to shoves.
The town menu displays mass. Select opens the field menu; choose DRIVE to tune all
seven physics values directly. A on a tuning row restores the fitted preset. These are relative
arcade handling values, not a real-world vehicle model; no enemy tank art or
dedicated tank setup yet.

Changing setups in town does not reset position or regenerate the world.
The selected setup carries across catalog maps. To retain a run across a reset,
open Select, press R three times for SAVE, and choose SAVE GAME. After reboot,
choose CONTINUE GAME on the title; the saved region is regenerated and the
profile is applied. Active races and local enemy/projectile
state are deliberately rebuilt rather than serialized. If the map catalog changed,
credits and completed-job progression remain, but an active contract is cleared
because its old map references may no longer be meaningful.

mGBA and RetroArch's mGBA core recognize the ROM's SRAM marker and store this
profile in their normal battery-save file (commonly .sav). Emulator save-states
also capture the SRAM contents and complete live machine state. Use SAVE GAME
for a portable profile; save-state files are emulator/version specific.

WHAT IS IN THIS DEMO
--------------------
- Native GBA ROM, C++ / Butano / devkitARM.
- Three catalog-selected 8k procedural maps with fixed seeds, connected floor,
  canyon walls, six settlement icons, live minimaps and a town/garage scene loop.
- Five pooled enemy drivers, typed regional spawn profiles, cooldowns, loot and despawning.
- Grouped normal weapons, a dedicated special trigger, three-hit enemies, player
  health, rechargeable shield and battery energy with authored energy-cell drops.
- Five selectable 64-direction pixel-art bodies: roadster, sand buggy, old car,
  truck and pickup. All share the original palette-swap indices and matching
  attachment footprint. Player mounts reflect the three garage slots; raiders
  carry their deck gun. The view remains fixed and elevated;
  there is no 3D renderer.
- Fixed-point momentum, speed-dependent steering, grip-limited sliding,
  braking, reverse, and slower curb shoulders. The handling presets are unchanged.
- Terrain collisions keep the car out of canyon walls and town buildings.
- Camera look-ahead and a fixed-2x circular overview with live enemy dots and
  three segmented arcs: health left, shield right and centered energy below.
- Skid/dust particles and synthesized motor, tire, impact, and UI sounds. The
  motor uses a low evolving rumble, narrow pitch range and sustained-driving
  fade so it supplies throttle feedback without dominating long trips.
- Title screen, pause/help screen and paused map/drive/music/save field menu.
- A manual versioned SRAM profile with CRC validation, alternating copies and a
  commit-last write. It preserves location, setup/tuning, body and battery,
  audio, inventory/crafting, vitals/loadout, credits, contract progress and records.
- Local browser workshop backed by the same shared map catalog compiled into the ROM.

This is the driving foundation for an original vehicle-adventure RPG,
inspired by Racing Gears Advance's handling and Car Battler Joe's vehicle
progression. It does not reproduce either game's code, artwork, sound,
maps, names, or exact physics. Manual cartridge persistence is available from
the development field menu, and a valid profile enables CONTINUE GAME on the
title. Automatic checkpoints are not implemented yet.

BUILD ON WINDOWS
----------------
Prerequisites: Git and running Docker Desktop using Linux containers.
PowerShell is sufficient; no global GBA compiler or Python install needed.

From this repository:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1

The script downloads Butano 21.8.0 at the pinned commit, builds a local
Docker image with the pinned devkitARM base, generates assets, and compiles
the ROM. The compiler and asset tools run inside the container, with the
checkout mounted at /work to avoid Windows path-with-spaces problems.

Output:
  dist/dustline.gba      Playable ROM
  dist/readme.txt        This document
  dist/LICENSE           Dustline's CC BY-SA 4.0 license
  dist/licenses/        Upstream runtime/dependency license notices
  dustline.elf           Local debugging symbols (not committed)
  build/                Intermediate compiler/asset outputs (not committed)

The first build needs internet access and downloads sizeable dependencies.
Later builds reuse .tools/butano and the local dustline-build:1 Docker image.
The project is hosted at https://github.com/abhuva/dustline; pushes to main are
built automatically and published at https://github.com/abhuva/dustline/releases/latest.
The local build script does not change global PATH settings.

To force a clean rebuild:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Clean

Do not edit .tools/butano to tune the game. It is an ignored external
dependency, pinned to commit c66094ae514c74068f992a4896c5e1f234e9f6e2.
The devkitpro/devkitarm base is pinned by SHA-256 digest in tools/Dockerfile.
The compiler reports devkitARM GCC 16.1.0. Debian helper packages are obtained
from Bookworm repositories when the local image is first built; their patch
versions are not frozen, so a future rebuild is not promised byte-identical.

WEB MAP WORKSHOP
----------------
Requires Docker Desktop and Python 3; the optional parity test also uses Node.js.
From the repository root, run these PowerShell commands:

  powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass -File .\map-editor.ps1

The first command only needs to be run once initially to generate the game art.
Keep the second command's terminal open, then visit http://127.0.0.1:8765 in a
browser. Press Ctrl+C in the terminal to stop the server.
The script compiles the shared C++ generator to WebAssembly using Emscripten
4.0.15, pinned by Docker digest. There are no frontend packages, CDN requests,
or external accounts. Everything runs locally, including generation in a worker.
After the first workshop build, this starts immediately when its C++/WebAssembly
engine has not changed:

  powershell -NoProfile -ExecutionPolicy Bypass -File .\map-editor.ps1 -NoBuild

Rebuild without -NoBuild after changing C++ generator code. -BuildOnly builds
the engine without starting the server; -Port 9000 chooses a different loopback
port. Maps saved with Include in game enabled are compiled into the ROM by the
next build.ps1 run.

The map toolbar's Shops dialog edits the recipe's tier floor, town tier range,
base stock size, family weights and per-town modifiers. Preview inventories are
resolved by the same Python catalog code used by the ROM build, so all six named
town rows show the exact ordered stock that will be compiled. Invalid or
undersized eligible pools are reported before saving or building.

MUSIC WORKSHOP
~~~~~~~~~~~~~~
With the same local server running, open:

  http://127.0.0.1:8765/music.html

The music page edits music/dustline-drive.json, the single source for browser
preview and GBA output. The current Dustline Horizon study provides four editable
16-step arrangements: Open horizon, Gathering speed, Distant threat and Storm
chase. Eight fixed channels cover dust kick, brush snare, sand ticks, low drone,
horizon pad, desert air, signal lead and glass chime. Click drum
cells to toggle hits. Click tonal cells repeatedly to cycle through the selected
scale; right-click clears a cell. Root, scale, BPM, deterministic variation seed,
B-pattern variation and per-channel volumes are editable.

Play loop previews unsaved changes immediately with Web Audio. Selecting another
section while playback runs queues it for the next 16-step boundary. Save +
generate uses a revision-checked local API and atomically updates the JSON source,
then generates audio/dustline_drive.mod, include/generated/music_data.h and the
reference WAV shown on the page. The WAV plays the exact expanded patterns and
synthesized samples in section order; browser live playback is an approximation
for quick composition. Run build.ps1 after saving to import the MOD into Maxmod
and copy the updated ROM to dist/.

The warm instrument bank replaces raw saw and pulse oscillators with a seamless
rounded bass, a long detuned chorus pad and circularly filtered wind noise. Pad
and air contain a click-free synthesized attack followed by a legal tracker
sustain loop. The lead and chime are longer one-shots whose upper partials decay
faster than their fundamentals. This makes sparse patterns hold a texture and
lets the recurring lead phrase remain recognizable while each reactive state
adds motion and weight.

The ROM uses an 8CHN ProTracker module at the existing 16 kHz Maxmod mixing rate.
Each state owns two 32-row/two-bar patterns; tracker pattern jumps loop the active
state. Runtime changes use Butano's sequence-position API at pattern boundaries,
so they do not consume the four sound-effect channels used by engine, weapons,
skids, impacts and UI. A three-second downgrade hold prevents musical flutter.
The public Butano API does not independently fade arbitrary tracker channels;
the four authored arrangements provide the layer combinations instead.

The editor provides a draggable node graph with typed input/output connections,
live intermediate previews, undo/redo, shared-library persistence, JSON import/
export, PNG captures, before/after comparison, floor-region overlays, exact
refined collision, game textures, and a nine-seed preview grid. Changing settings keeps the seed;
the refresh button chooses a new seed. Click a seed thumbnail to explore it.
The Map selector loads entries from maps/map-library.json. New starts a blank
draft, Save updates its catalog entry, and Save As creates a separate entry.
Drafts may be incomplete while Include in game is off. Enabling that checkbox
requires a complete valid graph; enabled entries are compiled into the connected
game world after the next build. Delete map removes the selected catalog entry. The local
server uses revision checks and atomic file replacement to prevent stale tabs
from silently overwriting newer saves. Unsaved edits are recovered per map in
the browser when the catalog revision still matches. Ctrl+S also saves.
Placements is the default preview. It uses a clean wall/floor/road/town schematic
instead of full terrain textures. World connections visibly add their reserved
gold corridor, boundary road, cyan trigger and pale derived arrival. Manual
Player Spawns have stable IDs, positions and headings for New Game, tests and
future special transport; drag or edit those cyan markers directly. Normal exits
are not placements and require no local trigger or arrival authoring.

Open World map for the separate region graph. Enabled maps are freely draggable
nodes with fixed N/E/S/W ports. Click a free port and then the complementary port
on another map to create one reciprocal connection; occupied or incompatible
ports are rejected. Bracketed coordinates are derived from topology and do not
change when a node is dragged. Contradictory cycles and same-cell collisions are
reported. Disconnected drafts may be saved, but a ROM build requires every
enabled map to be connected to the selected start. Connection edits and removal
are atomic in undo/redo. Atlas generates each clean 128 x 128 schematic and
stitches it at the derived Zelda-like grid coordinate. Disabling a map removes
its node and incident connections. Save world uses the same revision-checked
atomic API as map saves.
Drag between output and input ports in either direction to connect them; dropping
onto an occupied input replaces its connection. Outputs can feed multiple inputs.
Click the x beside a connected input to disconnect it. Escape or dropping on
empty space cancels a drag. Clicking output then input and the Node Settings
selectors also support connecting/replacing; reset a selector to clear its input.
Connection edits support undo/redo. Drag node headers to move nodes.
To tune one node while watching another, select the node to edit and click
Pin settings, then select a downstream node for its preview. The pinned controls
keep editing the named node and the preview updates live. Unpin settings restores
selection-following controls. Duplicate/Delete act on the node in Node Settings.
The temporary cellular iteration slider also works through downstream previews.
Pins clear when their node is removed or a different recipe is loaded.
The desktop workshop fits the browser window with a single compact toolbar and
graph heading. Drag empty graph space to pan without bounds and use the mouse
wheel to zoom around the pointer. The graph has no scrollbars. The preview scales
to the space available. Small windows or large text may require scrolling inside a panel.
The built-in Guide describes mask semantics and the individual controls.

Game textures previews the actual terrain art using the same C++ tile selector
as the ROM: rounded walls, cliff faces, ground patches and settlement stamps.
Full map render opens an 8192x8192 render with Fit/25%/50%/100% zoom and Save full
PNG. Cars, enemies, radar and HUD are not part of the terrain render. The small
live preview is downsampled; the full PNG contains every original 8px tile.
Full renders include temporary iteration-preview settings; JSON exports do not.
PNG rows are compressed incrementally to avoid a giant RGBA export buffer.

Per-map art banks
~~~~~~~~~~~~~~~~~
Art bank opens the graphics configuration stored with the current map recipe.
It lists the real assets from maps/art-assets.json with generated pixel previews.
Each map independently chooses four ground material assets, the byte ID produced
by generation for every enabled material, four foreground decoration assets,
one wall set and one town set. Material assets also declare their firm or loose
driving surface. Enabled material IDs must be unique; at least one is required.
Disabled decoration slots have zero effective weight in both preview and ROM.

The build creates a separate deduplicated terrain, decoration and palette bank
for each distinct in-game profile. Identical profiles share a bank. All banks
live in ROM, but only the selected map's bank is allocated in VRAM. Sun, Rust,
Ash and Salt reuse transformed source geometry. Twin Cities uses the fifth,
fully redrawn Verdant Machine bank: grass and moss floors, concrete and machine
plates, tree-canopy walls with industrial retaining edges, greenworks settlement
icons, ferns, flowers, pipe scrap and vents. It uses 167 of the same 192 reserved
terrain slots and an 80-colour compacted terrain palette.

Profile edits are ordinary map edits and participate in Save, Save As, import,
export and undo. Save the map, run build.ps1, then reload the workshop to repack
the ROM and refresh Game textures. Adding or changing source art also requires
that rebuild/reload cycle; the development tool intentionally has no file watcher.

Enemy spawns, populations and decoration (version 4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Choose the Populated wasteland preset or add Enemy spawns and Decoration nodes.
They are independent final outputs applied to the selected wall/floor world.
New nodes become their respective output automatically; Use as spawn/decoration
output selects another branch. JSON stores spawnOutput and decorationOutput.
The Wasteland catalog entry includes both outputs. Older recipes retain legacy
encounters and no decoration. Ground output remains optional. Version 3 maps
load with a default raider population profile.

Enemy spawns has Target count (0-258), Minimum spacing (0-1024 world pixels),
Starter encounters, and an optional 0-255 density field on input A. Zero forbids a
location; positive values are relative weights for selection without replacement.
Generation chooses from the existing reachable sector anchors and respects town
clearance. Count is a target: the preview reports the actual number available
after terrain, field and spacing restrictions. Starter encounters explicitly
requests the two original near-start anchors; they count toward the target and
respect both the field and spacing. Five active enemy cars remains the limit.
Input B is a categorical 0-255 population field. Its value selects a per-map
Population profile containing enemy type, respawn seconds, scrap chance/range,
and energy-cell chance/minimum/maximum. Constant field,
Stepped LUT and Paint field
value author uniform, banded and masked regions. Unmatched IDs use the first
profile. The Wasteland map demonstrates three north/center/south profiles.
The Spawns checkbox controls profile-colored markers in previews and full PNG
exports. These markers are editor overlays; in-game radar dots remain live enemies.

Decoration has Density %, four relative slot weights, Keep roads clear, and an
optional 0-255 density field on A. Art bank chooses the asset occupying each slot.
Effective density is Density % multiplied by the field / 255. The weights choose
the mix among placed patches; they need not sum to 100. All-zero weights or zero
density places nothing. Constant field makes a uniform value; Mask to field
converts a binary selection into 0/255. Noise, radial and other fields work too.
Each 32px cell has one hashed candidate, with an 8px-aligned random offset and
16px artwork. Independent hashes control presence, position and type. Weight
changes preserve positions; the same seed/stream reconstructs them after travel
or town visits. Walls, settlement stamps and optionally roads exclude patches.
They have no collision, grip changes, animation or saved state. Breakables are
not implemented in this pass.

Browser, native preview and GBA use the same placement code. Game textures and
full PNGs include the transparent artwork. The runtime streams it below cars on
a separate background layer, using no object-sprite slots. It visits newly
exposed 32px cells and caches visible results; it never scans the whole world
while driving. Placement checks cover masks, count/spacing, deterministic types,
native/WASM parity, PNG pixels and emulator rendering.

Version 2 recipes have two independent outputs: output (Playable world) and
materialOutput (Ground materials). Each output is a 64x64 byte array. Material
IDs cover 128x128 world pixels and are categorical bytes. IDs 0-255 are stored
unchanged. Art bank maps up to four enabled IDs to this map's selected ground
assets and driving surfaces. Unassigned IDs are flagged and use the first enabled
material as fallback. The real asset catalog lives in maps/art-assets.json;
maps/materials.json remains the legacy/default lookup used by portable fixtures.

Start with the Natural ground preset (also maps/recipes/natural-ground.json).
Its Value noise -> Stepped LUT -> Ground materials branch turns the input into
value bands, then interprets those values as material IDs. Position 0 is fixed;
its output is editable. Click the LUT histogram to add up to 255 movable points.
Select a point to edit its preview color and value/ID or delete it. Each point changes the output from its position
onward. Constant field creates a uniform base. Paint field value replaces
values wherever its mask is 1; chain these nodes for more material areas.
Select a Ground materials node and choose Use as ground output. The Playable
world node remains the wall/floor output. Pin material settings while previewing
Final world / Game textures to tune areas live. Material edges follow the 64x64
cells; no blending is applied. Wall and settlement artwork retain their own
textures, including the sand already drawn into the settlement stamps.

Town roads
~~~~~~~~~~
Use the Natural ground + town roads preset, or connect Playable world -> Town
roads and select the latter as the wall/floor output. Ground materials remain
independent. Pin the road settings while previewing Final world / Game textures.
The node runs one breadth-first search from the starting town, then follows the
same parent tree from every other town. Equal-cost cardinal steps give shortest
valid routes; ties use stable N/E/S/W ordering. There are no terrain preferences.
Connections use N/E/S/W bits in one byte per logical cell.
Roads do not change walls, placements or ground IDs. The material overlay changes
both ground art and grip; it supports the same 0-255 IDs and fallback catalog.
Road width is a single value from 8-256 world pixels, sampled at the game's
8px tile resolution. Width variation is disabled to keep GBA runtime cost low.
Old min/max exports import at the midpoint, rounded to an 8px tile (equal limits
retain their original value). The active export uses 160px instead of 153-170.
Wide roads extend into neighbouring cells, with walls and settlement artwork
retaining priority. Narrow passages can therefore clip the requested width.
Axis-aligned segments have rounded joins. Their anchors sit south of the town buildings;
north approaches at town cells are excluded, with routes using their open plazas.
The paths are shortest on this valid road graph. They are not diagonal paths or
smoothed splines. Rebuild to use exported road settings in the game.

The Town roads catalog entry demonstrates constant-width roads and ground settings.
Run ./build.ps1 after saving to include it in the ROM.
Current full terrain previews are artifacts/map_editor/roads-full-map.png and
active-full-map.png. Road storage in the shared source is 4,104 bytes;
the generation workspace is 45,136 bytes and reuses the existing flood queue.
Road search runs only while generating the map. Tile/grip lookup checks four
surrounding anchors with integer arithmetic; there is no width noise,
interpolation or runtime width grid. See artifacts/test-results.json for
measured GBA frame timing. Default recipes without this node retain
their existing appearance and allocate no persistent road grid.

Version 1 recipes keep the original 512px patch rules and sand around outposts.
Assigning a ground output switches to version 2 and makes its IDs authoritative
for floor art and grip, including ground around settlements. The Ground materials
inspector offers Use original ground rules to switch back. Deleting the active
ground output also restores those legacy rules; Undo restores the output.
Material assets also carry a four-way driving character. Sand/dust/grass is the
loosest surface, with slow lateral wander and long slides; gravel/shingle/moss
has shorter chatter and moderate slide retention; hardpan/yard gives a light
rumble and small lateral movement; asphalt/road stays stable. The road overlay
and driving physics use this same material query.

Operations: random fill; arbitrary cellular birth/survival rules, 4/8 neighbours,
0-32 iterations, optional update mask and border; radial field; threshold;
wall-mask union/intersection/subtraction/XOR; weighted field blend with optional
mask; invert; largest four-connected floor; integer value noise with octaves;
diamond-square plasma; squared Voronoi nearest-site / F2-F1 distance; and a
Playable world finalizer. Source nodes have stable independent random streams.
The CA iteration preview is temporary and does not alter the exported recipe.
The finalizer enforces a two-cell solid border, retains connected floor, reports
fallback clearing use, and carves the original spawn and six outpost approaches.

To use a recipe in the ROM:
  1. Create or load a map in the workshop and choose its final outputs.
  2. Turn on Include in game and press Save or Save As.
  3. Run ./build.ps1, then ./test.ps1 and playtest the resulting ROM.
The local server writes maps/map-library.json. The asset generator compiles every
enabled entry into include/generated/wasteland_recipe.h.
Do not hand-edit the generated header. The ROM executes the recipe at runtime;
no layout bitmap is baked into it. Each entry always uses its saved seed. The
default Wasteland entry has seed 12648431, fixed road width 160px, an independent
ground-material branch, spawn points and decoration. Standalone JSON import and
export remain available for exchange and backups; they do not change the shared
catalog until saved through the workshop.
Graph positions and labels are editor-only; unconnected branches are not exported.

Recipes are bounded: 64x64 logical cells, 64 total nodes, six live
4 KiB grids per output. The two programs run sequentially in the same workspace. The executor reuses buffers after their last consumer. CPU cost depends
on the operations and iteration counts. Browser timing is not a GBA estimate.
Arbitrary recipes still need clearance, settlement-spacing and driving playtests;
the editor renders terrain but does not simulate vehicles or gameplay.
The existing gameplay routes assume the original layout, so a substantially
different exported recipe may require new controller-driven test routes.

Run ./test-map-editor.ps1 -NoGba to rebuild and test just the dev tool (native
and WebAssembly checks, with no GBA compilation or emulator run).
Run ./test-map-editor.ps1 for native/WebAssembly/GBA parity. It compiles a separate
diagnostic ROM in build/, runs it in mGBA, and compares every result byte for 339
cases, including every preset stage, extreme seeds, masks, and 128 historical
maps with their placements. It also checks graph validation, buffer liveness,
rules, flood-fill ties and fallback behavior. Seven rendered maps are compared
against native output across all 1,048,576 tile references; sampled tile/surface
signatures also run on GBA. Full PNG files are decoded and their source pixels
checked. It does not change dist/dustline.gba.
For the DOM integration checks (no browser rendering), install the pinned test
helper and run:
  npm install --prefix build/map-editor-ui --no-save --package-lock=false jsdom@26.1.0
  node tools/test_map_editor_ui.mjs
To test the Natural ground catalog entry:
  docker run --rm --mount "type=bind,source=$PWD,target=/work" dustline-build:1 python3 tools/test_material_rom.py
Run ./test-map-editor.ps1 first for its render fixtures; ./test.ps1 supplies the
emulator bridge. For Town roads, run the same Docker command with
tools/test_road_rom.py. It also checks driving across the road
overlay and the additional grid allocation/release.
The separate material-ROM smoke test checks terrain pixels, grip, restart,
scene release and a short controller-driven route; subjective handling still
needs human playtesting.
Reports and editor screenshots live in artifacts/map_editor/. Generated editor
engine files live in tools/map_editor/generated/ and are ignored by Git.

VERIFY
------
After building:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1

The test script runs the actual dist ROM in headless libmGBA 0.10.1, sends
normal GBA joypad input, and reads exported telemetry through ELF symbols.
It never teleports the car or writes game state to make tests pass.

The current build and complete host/ROM suite pass. See artifacts/test-results.json
and the focused result files for exact results and the tested ROM hash.

It checks the New Game/conditional Continue title flow, every enabled region's
fixed seed, repeatable generation, acceleration, camera look-ahead, pause and frame budget. Exact
results and peak timings are recorded in artifacts/test-results.json, excluding
car, UI and particles from terrain pixel comparisons.
The procedural generator has a 128-seed host sweep: determinism, independent
four-connected flood-fill verification, placement separation, car clearance,
and traversable coarse-cell links. ROM tests compare the GBA's seed/signature
with that same host generator, exercise Yes/No and repeated town visits, exact
return coordinates, held-button protection, catalog switching, canyon collisions
and rendered pixels. Loading is separate from driving-frame profiling.
Tests also check the visible town artwork above its prompt, the dispatch contract
board and deterministic mission lifecycle, every overview
cell, town dots, player-marker alignment, one-upload stability while driving,
newly exposed HUD space, and town-only setup controls. Vehicle-lab tests cover
all handling properties, three battery capacities, five body sheets and their shared palette
contract, body selection/reset, garage slot fitting and compatible inventories,
wide-range clamps, preset reset, frozen motion, town persistence and the fixed
2x minimap.
Combat tests check three-hit destruction, enemy movement/firing, health, shield
and energy, terrain impacts, bounded projectiles, menu freezing and town persistence.
Traffic tests leave the player idle for 25 seconds, verify ongoing enemy motion,
vehicle sensing, forward-only fire and separation, then ram using joypad input.
Host tests exercise the exact rectangle/impulse math with 25 mass combinations:
momentum, energy loss, glancing hits, separation and terrain-pinned contacts.
The same 128-seed sweep verifies unique, reachable encounter anchors and sector
coverage. Spawning tests kill a car, wait through its cooldown, then drive out
of the district and back using joypad input. They check five-slot ownership,
despawning, off-screen activation, expired-kill respawn and damaged-survivor HP.
See the current results JSON for exact check counts and frame timings;
these are measured routes, not worst-case guarantees for every seed/system.

Evidence:
  artifacts/test-results.json  Assertions, telemetry, and tested ROM SHA-256
  artifacts/title.png         Native emulator title screenshot
  artifacts/driving.png       Native emulator driving screenshot
  artifacts/controls.png      Native emulator help screenshot
  artifacts/cornering.png     Native emulator cornering screenshot
  artifacts/wasteland/       Catalog snapshot, overview, tiles, emulator captures,
                              generated seed fixtures and art budget report
  artifacts/settings/        Vehicle lab, battery, extreme tune and HUD captures
  artifacts/weapons/         Fixed trigger groups, homing missile and curved energy HUD
  artifacts/combat/          Enemy/gunfire captures, route GIF and combat results
  artifacts/spawning/        Cooldown, streaming and damaged-survivor captures
  artifacts/traffic/         Anti-idle manoeuvres GIF, ram capture and results

Screenshots also have nearest-neighbor 4x versions. Actual rendering is
240 x 160 at the GBA's approximately 59.73 Hz. The GIF is sampled, so it does
not represent the display frame rate. Speed units are an arcade display
scale, not a real-world vehicle simulation.

These checks establish a working ROM and consistent mechanics. Subjective
handling feel and audio balance still need your controller playtest. This
build has not been tested on a physical cartridge or inside your RetroArch
installation. Its native ROM was verified using the mGBA emulator core.

PROCEDURAL WASTELAND PIPELINE
-----------------------------
See maps/overworld/README.md for generation rules and art provenance.
maps/overworld/wasteland-kit-muted.png and wasteland-details-muted.png are the
current environment sources, generated with the built-in image tool from the
supplied muted wasteland style reference. muted-art-prompts.txt records both
prompts. The build extracts, downsamples, quantizes and deduplicates the art
through tools/wasteland_assets.py and tools/generate_assets.py. The earlier
source kit and maps/open_world/wasteland.png are preserved.
Each map bank contains four ground materials and two wall textures repeating at
32px. Town stamps are 64px; selected details use transparent 16px patches. The editor's
Game textures / Full map render views use the same artwork as the ROM.
artifacts/wasteland/muted-art-proof.png shows 3x3 texture repeats and all four
details composited over each ground material. Alpha is binary on the GBA.

The on-GBA generator operates on a 64 x 64 byte grid, one cell per 128 world
pixels. It seeds 47% walls, applies five 4/5-neighbour smoothing passes, keeps
the largest four-connected floor region, then adds connected spawn/outpost
clearings. Convex wall corners round inward on the same 8px grid used for
graphics and collision. All coarse floor remains open; passages are at least
128px wide before the car footprint. Loose ground wanders and retains slides,
rough ground chatters, hardpan gives a light rumble and road remains stable.
Surface boundaries are deliberately simple
patches, not an authored road network. Town icons have small solid footprints
north of their accessible entrances. Future required objects must be placed
in this retained floor region and checked against those footprints.

Persistent layout is 4,168 bytes, plus 4,096 bytes for a version 2 ground output
and 4,104 bytes when Town roads is enabled. The bounded recipe workspace is
45,136 bytes (six grid buffers, 12,288 bytes of scratch, a staging layout and
road grid) and is freed before scene graphics load. There is no full 8k bitmap in RAM/ROM.
Terrain pixels and collision are evaluated from the coarse layout as needed.
Each current bank has 180 unique 8px artwork tiles in a 192-slot reservation:
12 KiB terrain VRAM,
18 KiB including the streamed map and driving HUD. The modal adds 4 KiB; the
decoration layer adds 6 KiB of background VRAM and about 4.5 KiB of streaming RAM.
The complete wasteland tile set uploads during scene loading and stays resident.
Driving updates only newly exposed tile-map rows/columns, with no artwork
eviction or whole-view cache pinning.
Wasteland HUD, pause, town screens and decoration now use 4bpp. Decoration pixel
data shrinks from 1,088 to 544 bytes; its allocated VRAM still rounds up to a
2 KiB block, so normal driving remains at 24 KiB total background allocation.
UI pixel storage is also halved (maps remain 16-bit). Per-bank lossless RGB5
palette compaction uses 80-112 terrain entries, leaving separate
16-colour banks for UI and scenery. Terrain stays 8bpp: 91/180 current tiles
need more than 15 opaque colours.
Its density/settings occupy 4,120 bytes, and saved spawn coordinates use 1,036
bytes. The temporary spawn weight field is freed after generation. Decoration
art has 17 transparent 8px tiles with its own muted 16-colour palette bank. The
blank town uses only 4 KiB total background VRAM. The fixed-2x 64x64 overview uses
2 KiB of sprite VRAM, 2 KiB of staging RAM, a 4 KiB source image and a dedicated
16-color palette. Five enemy dots share one 32-byte tile and the existing sprite
palette; the circle masks take 4 KiB of ROM. Combat uses five
existing directional enemy-car frames and a shared enemy palette. Each selectable
player body adds 32,800 bytes of ROM for 64 headings but only its current 32x32
frame occupies sprite VRAM; all bodies reuse the same attachment overlay and
palette layout. Combat otherwise uses small HP/effect sprites,
and fixed pools of 24 bullets, two missiles and six traps. Its simulation state
is 4,716 bytes in a single
boot-time EWRAM allocation (not on the small internal stack), with separate
small sprite handles; there is no per-frame allocation or unbounded projectile
list. All AI and weapon simulation is fixed-point and separate from presentation.
Diagnostic telemetry lives in EWRAM to preserve the small internal stack.
This includes a 3,100-byte bounded encounter table (capacity 258; 12 bytes per
anchor plus a count). Inactive anchors run no physics or AI. Nearby anchors are
scanned every eight driving frames and at most one car is activated per scan.
Wasteland overview generation reads only wall cells and road-connection bits;
there are no terrain/material samples while driving. Its crop reuses packed rows
and a precomputed circle mask, updating only on display-pixel crossings.
Town entry releases the terrain renderer and radar, retaining only the run
layout, car/camera state and shared UI/car/particle resources. Returning rebuilds
graphics from that same layout; it does not reroll the world. Selecting a new
catalog entry releases the previous layout and regenerates from the saved seed.

This is a first reusable art pass: texture repetition, coarse map-scale shapes,
simple material boundaries and conservative town hitboxes remain visible.
The layout is connected, but route variety, travel times, wall readability and
cornering comfort need human controller testing. Manual profiles persist the
current region and coordinates; automatic world checkpoints are not implemented.

PROJECT FILES
-------------
AGENTS.md                  Development direction and agent instructions
Makefile                   GBA build configuration
build.ps1 / test.ps1        Build and emulator verification entry points
include/driving.h          Vehicle data, tuning presets, and physics interface
src/driving.bn_iwram.cpp   Motion, surfaces, and collision response (ARM hot loop)
src/main.cpp               Input, camera, presentation, lap rules, and audio
include/save_data.h        Stable versioned profile schema and redundant slot format
src/save_data.cpp          Explicit byte codec, validation and CRC implementation
src/save_store.cpp         Butano SRAM reads, alternating writes and erase support
include/adaptive_music.h   Quantized section switching and intensity hysteresis
src/adaptive_music.cpp     Maxmod playback, boundary jumps and menu ducking
include/mission.h          Bounded session contract state and lifecycle interface
src/mission.cpp            Deterministic courier/hunt generation and completion
tools/generate_assets.py   Original procedural graphics, map data, and audio
music/dustline-drive.json  Editable tracker motifs and generator settings
tools/music_generator.py   Deterministic MOD, section header and WAV generator
src/world_map.cpp          Selected scene, ROM tile lookup and surface queries
src/terrain_streamer.bn_iwram.cpp  Terrain tile cache and VBlank uploads
maps/map-library.json      Shared editor and ROM map catalog
tools/compile_recipe.py    Catalog validation and generated C++ recipe compiler
tools/serve_map_editor.py  Loopback map/music server and atomic save APIs
tools/emulator_bridge.c    Thin headless mGBA adapter for integration tests
tools/test_rom.py          Joypad-driven checks and capture generation
include/cave_layout.h      Shared integer cellular automaton and flood fill
src/wasteland.cpp          Runtime procedural tiles and surface queries
src/local_minimap.bn_iwram.cpp Fixed-2x wasteland image, vitality/energy HUD and radar dots
tools/wasteland_assets.py  Reference-derived tile kit compiler and budget report
tools/town_assets.py       Native town backgrounds and walking sprite generation
include/town_scene.h       Walkable town/interior state and interaction interface
src/town_scene.cpp         Town movement, collision, doors and mechanic menu
include/weapon_fitting_scene.h Dedicated fitting-screen state and interface
src/weapon_fitting_scene.cpp   Mount selection, inventory, preview and weapon data
tools/test_town_scene.py   Controller-driven town/garage vertical-slice test
tools/test_missions.py     Controller-driven dispatch-board and contract UI checks
tools/test_mission.cpp     Host contract generation and event-lifecycle checks
tools/test_cave_layout.cpp Host seed sweep, clearance checks and seed export
tools/test_wasteland.py    Procedural graphics and town lifecycle ROM tests
tools/test_settings.py     Vehicle lab, batteries, audio, fixed HUD and persistence tests
include/combat.h           Combat limits, enemy/bullet state and weapon tuning
src/combat.cpp             Fixed-point enemy driving, weapons and damage
src/combat_view.cpp        Existing car art, HP pips, bullets and destruction FX
tools/test_combat.py       Joypad-driven combat and scene-lifecycle checks
include/enemy_spawns.h     Seeded reachable encounter anchors and compact state
tools/test_spawning.py     Cooldown, off-screen spawning and out/back streaming tests
include/vehicle_contact.h  Shared fixed-point rectangular contact/impulse solver
tools/test_vehicle_contact.cpp Host contact and mass-response tests
tools/test_traffic.py      Anti-idle AI, car sensing, front guns and ram ROM tests
tools/Dockerfile           Isolated compiler and test dependencies

Generated graphics/, audio/, and include/generated/ are deliberately ignored
by Git. Regenerate them through build.ps1. The asset generator is their source
of truth. Tuning acceleration, grip, top speed, and steering starts in
include/driving.h.

CREDITS / THIRD-PARTY SOFTWARE
-----------------------------
Game-specific code, pixel artwork, font, layout, and synthesized sounds were
created for this project. There are no extracted commercial-game assets.

Butano by Gustavo Valiente and contributors:
  https://github.com/GValiente/butano
devkitARM / devkitPro:
  https://devkitpro.org/
mGBA by endrift and contributors (test tooling):
  https://mgba.io/
Pillow (asset generation and screenshots):
  https://python-pillow.org/

Except for separately identified third-party material, Dustline's game-specific
code, artwork, audio, documentation and compiled ROM are licensed under the
Creative Commons Attribution-ShareAlike 4.0 International License. Credit
"Dustline by Marc Bielert", link to the project and license, and indicate changes.
See LICENSE and https://creativecommons.org/licenses/by-sa/4.0/ for the terms.

dist/licenses contains Butano's upstream dependency notices. Some notices cover
optional backends not enabled by this build. Third-party components remain under
their respective licenses.
