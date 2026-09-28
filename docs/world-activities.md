# Dustline world activities

**Status:** Living design notes; radio-barrel prototype and direct garage shop implemented  
**Last updated:** 2026-09-28  
**Participants:** Marc Bielert, Codex

## Purpose

Dustline's regions are deliberately large. They should not become fields of
anonymous pickups or empty travel between towns. World activities should give
the player reasons to drive differently: stay on a road, leave it, search,
race, fight, protect something, take a risk, or return later with a better car.

The preferred activity loop is:

1. Discover something while driving.
2. Decide whether and how to engage with it.
3. Use driving, shooting, or interaction to resolve it.
4. Receive a useful reward or leave a persistent change in the world.

Activities should reuse a small number of systems in different combinations.
The game should not require a unique subsystem for every point of interest.

## Design principles

- Make travel itself part of the activity, not dead time before it.
- Prefer readable situations over a large quantity of map icons.
- Use rewards that feed vehicles, weapons, the garage shop, missions, and towns.
- Mix authored locations with streamed or seeded events.
- Keep interactions short enough that stopping the car remains a meaningful
  choice rather than constant interruption.
- Let discoveries and cleared locations persist where practical.
- Use roads, terrain, landmarks, and shortcuts as part of activity design.
- Telegraph danger and opportunity clearly at GBA resolution.
- Avoid punitive survival meters unless they create interesting driving choices.

## Regional shop progression

Town shops are planned to use stable regional item pools rather than showing the
complete catalog everywhere or scaling with player level. Each town will have a
deterministic base inventory derived at build time from item region policies,
tiers, a regional profile, and optional town modifiers. Runtime ownership only
removes purchased items; shops do not reroll or backfill them. This makes travel
to new maps part of equipment progression while allowing players to learn where
an unaffordable item is sold and return later.

See [ADR-004](adr/ADR-004-deterministic-regional-shop-inventories.md) and the
[shop inventory design](shop-inventory-progression.md) for the complete proposal.

## Activity families

### Salvage and discovery

- Loose scrap can be collected by driving through it.
- Wrecks can require the player to stop nearby and salvage, leaving the car
  exposed for a short time.
- Armored caches can be opened by shooting them.
- Buried stashes can be found by following radio signals or clues.
- Rare parts can unlock a specific weapon or vehicle part directly.
- Abandoned cargo can be collected and delivered. Its weight can alter handling
  until it is handed in.
- Rare vehicle wrecks can be discovered in the world and restored at a garage.
- Salvage sites can contain a choice between a quick small reward and a slower,
  riskier extraction.

Collection is most useful when it creates a new situation. Carrying cargo,
discovering a restorable car, or attracting enemies is preferable to merely
incrementing a counter.

### Events during travel

- A merchant or fuel truck travels between towns.
- A stranded driver requests parts, a tow, or an escort.
- Bandits chase a civilian convoy.
- An ambush uses barricades, mines, and parked gun cars.
- Two factions are already fighting when the player arrives.
- A vehicle flees with stolen cargo and must be intercepted.
- A distress signal can be genuine or a trap.
- A storm, rockslide, wreck, or roadblock temporarily changes a route.
- A rival driver challenges the player without requiring a visit to a menu.

These events can originate at authored anchors and only be simulated when the
player is nearby. The whole region does not need permanently active actors.

### Driving activities

- Speed traps with persistent bronze, silver, and gold records.
- Drift zones through authored bends.
- Jumps with distance records.
- Off-road hill climbs.
- Time trials between landmarks.
- Destruction runs with a sequence of targets and a time limit.
- Deliveries that reward speed, fuel efficiency, or avoiding damage.
- Fragile cargo whose value drops after collisions.
- Heavy cargo that changes acceleration, grip, and braking.
- Pursuits where escape is the objective rather than destruction.
- Shortcut challenges in which the safe road competes with a risky off-road
  route.

Permanent records make familiar roads worth revisiting with different cars and
setups.

### Major points of interest

- Bandit camps with gates, defenders, generators, turrets, and a central cache.
- Scrapyards occupied by hostile salvagers.
- Radio towers that reveal nearby activity areas.
- Abandoned garages that become repair or spawn locations.
- Fuel depots that can be captured, protected, or destroyed.
- Caves, tunnels, dried reservoirs, crashed aircraft, and ruined settlements.
- Arenas with repeatable combat or driving challenges.
- Boss vehicles roaming a defined territory.
- Strongholds that require upgrades or several visits to clear.

A camp can support multiple approaches. Destroying its generator may disable
turrets; breaking through the gate may be faster but more dangerous; searching
the perimeter may expose a back entrance.

### Persistent world changes

- A cleared roadblock stays removed.
- A bridge can be repaired with collected material.
- A restored radio tower reveals discoveries in its coverage area.
- A liberated outpost provides repairs, jobs, or a player spawn.
- Defeating a gang leader reduces ambushes in the surrounding sectors.
- Helping a settlement causes merchants or friendly traffic to appear.
- Choosing which faction controls a depot changes its services and rewards.

Even modest persistent changes help a region feel like a place rather than a
level that resets after every visit.

## Roads and off-road space

Roads should do more than provide better grip:

- Merchants, convoys, racers, and patrols primarily travel on roads.
- Ambushes and roadblocks use authored choke points.
- Deliveries reward fast and undamaged road travel.
- Off-road shortcuts save time but add rough terrain, enemies, or navigation
  risk.
- Clearing nearby camps can make a road safer.
- Portal-to-road links allow inter-region routes to continue naturally.

The proposed workflow keeps the World Map node canvas free-form but gives every
node four fixed cardinal side ports. Shared connections derive a logical region
grid and apply reciprocal midpoint exit overlays to otherwise exit-free map
recipes. Each overlay reserves a wall-free corridor, carves toward the primary
open area when needed, joins the closest reachable road, paints that road to the
exact boundary, and derives its arrival from the opposite side. The browser can
then stitch every 128x128 schematic into a complete world atlas.
See [ADR-005](adr/ADR-005-cardinal-region-grid.md) and the
[cardinal world-grid design](cardinal-world-grid.md).

Off-road areas should contain discoveries and shortcuts rather than merely
slower copies of the road network.

## Suggested content density

These are starting targets, not hard rules:

- A micro-event every 15–30 seconds: scrap, a wreck, or a small enemy group.
- A distinct activity every 1–2 minutes: a signal, convoy, challenge, or ambush.
- A major landmark every 3–5 minutes: a town, camp, garage, cave, or boss area.

A sector may select only a few encounters from its authored pool for a given
run. Discoveries and major cleared locations can remain persistent even when
minor events are refreshed.

## Radio-signal discovery prototype

### Equipment integration

The radio-signal receiver is a special garage item for the vehicle's top slot,
where it directly competes with missiles and traps. It has its own equipment
icon and its installed state is saved as part of the vehicle loadout.

- The signal overlay and automatic signal tracking operate only while the radio
  receiver is fitted.
- Replacing the receiver with a missile, trap, or empty slot removes the overlay
  immediately.
- The first receiver is passive and costs no energy. Its cost is giving up the
  active top-slot weapon.
- `L` does nothing while the receiver is fitted. It may become a signal-cycle
  control later if manual target selection proves necessary.
- Hidden barrels exist whether or not the receiver is installed. Without it,
  they can only be found through ordinary visual exploration.
- The radio receiver and fitted state follow the existing inventory, garage,
  regular-save, and emulator-save-state behavior.

The implemented progression makes the receiver a direct garage-shop purchase.
A new game begins with only the standard front gun; after buying the receiver it
appears in the top-slot fitting inventory alongside bought missiles and traps.

This is an intentional loadout decision: a combat-focused car can take a missile
or trap, while an exploration-focused car can sacrifice that weapon to locate
hidden activity rewards.

### Player experience

The first prototype is a hidden salvage signal. It deliberately does not place
an exact marker on the minimap.

1. The player equips the radio receiver in the garage's top special slot.
2. The player enters the detection range of an active signal source.
3. A chevron indicator appears a short distance from the player's car.
4. The indicator occupies one of eight directions and points toward the source.
5. Its strength increases from one to three chevrons as the player approaches.
6. At close range, a visible barrel is streamed in at the source position.
7. The player shoots the barrel. It disappears and its reward is collected.
8. The signal stops and that pool slot schedules a new random source after a
   three-to-five-minute driving-time delay.

The mechanic asks the player to interpret direction and strength, then visually
search the nearby terrain. It should feel like homing in on a transmission, not
following a conventional waypoint.

### Direction indicator

- Use eight world-relative directions: N, NE, E, SE, S, SW, W, and NW.
- The fixed elevated camera means world-relative and screen-relative directions
  are the same. The indicator should not rotate with the car's heading.
- Display only the direction of the currently tracked signal, never all eight
  positions simultaneously.
- Position the indicator approximately 24–32 pixels from the car, far enough
  not to obscure it or be confused with car weapons.
- Render above terrain, vehicles, effects, and the HUD elements it overlaps.
- Use one, two, or three oriented chevrons to communicate signal strength.
- Apply a small angular hysteresis at the eight direction boundaries so the
  indicator does not flicker between adjacent directions while the car moves or
  slides near a boundary.
- Reserve color and animation for readability. A small pulse can distinguish
  the indicator from bullets without adding text.
- Keep the indicator inside the safe screen area when the car or camera is near
  a map boundary.

The exact art may use repeated small chevron sprites or a composite sprite for
each direction and strength. The result should look like `>`, `>>`, or `>>>`
when pointing east, with corresponding rotated forms for the other directions.

### Initial distance bands

Values must be tuned through playtesting. A reasonable first pass is:

- No signal: every source is farther than the hard 1024-pixel detection range.
- Weak (`>`): source detected at long range.
- Medium (`>>`): the player is within roughly half the detection range.
- Strong (`>>>`): the player is near the source and should start looking for
  the barrel.
- At very close range the strong indicator may remain until collection. If it
  covers the barrel or makes the final search trivial, it should instead pulse
  or disappear inside a small final radius.

Distance thresholds should include hysteresis so the display does not rapidly
alternate between strengths when the player sits on a boundary.

### Selecting a signal

The prototype tracks one signal at a time and chooses the nearest source inside
the detection range. A tracked signal is released when one of these conditions
occurs:

- it is collected;
- the player leaves its hard detection range;
- it becomes invalid; or
- another signal is substantially closer for a sustained period.

Manual signal selection can be considered later if regions become dense enough
to need it.

### Barrel target

- The first target is a clearly readable static barrel sprite.
- It has a world position and a bullet hitbox.
- It does not need to block the car in the first prototype.
- One player projectile destroys it initially; durability can be added later.
- Destruction removes the sprite and hitbox immediately.
- Destruction grants the reward directly for now; no second pickup is required.
- A brief flash, debris puff, sound, and reward message should confirm success.
- The first barrel should not explode or damage the player, keeping the
  discovery loop distinct from combat hazards. Explosive variants can be added
  later with clearly different art.

### Relationship to existing goals

Mission destinations, race checkpoints, portals, and other explicit objectives
can continue using their existing minimap/full-map markers. Radio discoveries
use the near-car chevron UI because uncertainty is the point of the activity.
The radio system should not silently replace navigation for explicit missions.

### Placement, streaming, and persistence

- Each visited map maintains three to five active sources in bounded run-local
  storage.
- Choose positions pseudo-randomly from reachable floor, away from towns,
  portals, the entry area, and other active sources.
- Instantiate the chevron only for the currently tracked signal.
- Instantiate the barrel only within its visual/interaction streaming range.
- Destroying a barrel leaves its slot empty for a randomized three-to-five-minute
  cooldown, then places a replacement at a new valid position.
- Count cooldowns in driving frames across the run. Preserve map pools while
  travelling between maps so portal hopping cannot force replacements.
- Do not store signal locations or cooldowns in the normal game save. Loading a
  normal save starts fresh activity pools; the equipped receiver still persists.
- Emulator save states naturally preserve the complete current runtime state;
  this includes exact positions and remaining cooldowns.

The implementation sizes its per-map state from the compiled map catalog rather
than imposing an unrelated map-count limit.

### Prototype reward

The first barrel should grant a simple, visible reward such as scrap. The amount
should be large enough to verify the loop but not require economy balancing yet.
Later signal sources can reveal rare parts, cargo, traps, story
fragments, or the start of a larger encounter.

### Prototype acceptance criteria

- The radio appears as a distinct selectable item for the garage's top slot.
- Equipping it persists through the same flows as other top-slot equipment.
- No radio overlay is shown when the top slot contains a missile, trap, or is
  empty.
- Replacing an active radio removes its overlay immediately.
- The receiver consumes no energy and `L` does not fire another top special.
- Tracking starts automatically when an active source enters the 1024-pixel
  detection range; the player never has to hold `L` to scan.
- An eligible signal is not shown as an exact minimap marker.
- The chevron chooses the correct one of eight directions while circling the
  source.
- Strength changes reliably with distance and does not flicker at thresholds.
- The overlay remains legible above terrain, cars, projectiles, and effects.
- Only the selected signal's indicator is visible.
- The barrel appears at close range and has a matching projectile hitbox.
- Shooting it once removes it, grants the reward, and stops the signal.
- A nearby barrel can still be found and destroyed without the receiver fitted.
- Leaving and returning retains the current pool and does not bypass cooldowns.
- A replacement appears at a different valid location after three to five
  minutes of driving time.
- Normal save/load creates a fresh pool; emulator save-state restoration keeps
  the exact current pool and cooldowns.
- Driving frame time remains within budget while signals are active.

## Expansion path

The signal framework can later lead to more than barrels:

- A wreck requiring a timed salvage interaction.
- A cache guarded by enemies.
- A distress beacon that begins an escort.
- A trap or ambush using a false signal.
- A fragment that reveals another signal elsewhere.
- A lost vehicle that must be driven or towed back.
- A radio tower that reveals approximate signal areas.
- A moving signal attached to a merchant, courier, or fleeing target.

The initial implementation should keep the node and completion model general
enough for these targets while only implementing the barrel behavior.

## Open questions for playtesting

- Does the initial 1024-pixel detection range create enough scouting without
  requiring overly dense coverage?
- Should the strong indicator disappear for the final visual search?
- Should the indicator pulse faster with strength in addition to adding
  chevrons?
- Is the nearest-signal lock sufficient when two discoveries overlap?
- Should hitting the barrel with the car count, or only player projectiles?
- How much scrap makes the detour feel worthwhile?
- Should towers reveal approximate signal regions, increase detection range, or
  both?
- How should the receiver be acquired once prototype testing is complete?
- Does the initial three-to-five-minute replacement delay feel rewarding without
  becoming a reliable scrap-farming route?
