# Cardinal world graph and derived edge exits

**Status:** Implemented
**Last updated:** 2026-09-28  
**Related decision:** [ADR-005](adr/ADR-005-cardinal-region-grid.md)

## Authoring workflow

1. Create and tune a map normally, without exits or transition arrivals.
2. Add or enable the map in the World Map. Its node always has north, east,
   south, and west ports and may be placed freely on the editor canvas.
3. Connect a side port to the complementary side of another map.
4. Save the World Map. The connection becomes reciprocal world topology.
5. Return to either map preview. The tool combines the base recipe with its
   current world connections and displays the generated opening, trigger,
   corridor, and road continuation.
6. If the generated result is unattractive, change the map seed or base design
   and preview again. No Portal or arrival needs to be repositioned.
7. Use a derived atlas view to inspect the complete Zelda-like region layout,
   independently of how nodes are arranged on the editing canvas.

## Required invariants

- Every World Map node exposes exactly four normal side ports.
- A port has at most one connection; a map has at most four normal neighbours.
- North pairs only with south, and east pairs only with west.
- A connection is reciprocal and is saved as one object.
- Free canvas coordinates affect editor presentation only.
- Logical grid coordinates are derived from connections, not authored by moving
  nodes.
- Every route through a graph cycle must derive the same logical coordinate.
- Two maps in one logical component cannot derive the same coordinate.
- Exit geometry and arrival are derived from the side and map dimensions.
- The generated road reaches the exact boundary through car-width clear floor.
- Randomness cannot change topology for the same recipe, seed, and world graph.
- Disabled maps have no compiled node or incident connections.

The four-neighbour maximum is an intentional world-structure rule, not a GBA
hardware limit.

## Terminology

**World Map node**  
An enabled map plus free-form canvas coordinates used by the browser editor.

**Side port**  
One fixed north, east, south, or west connection point on a World Map node.

**Shared connection**  
One reciprocal relationship joining two complementary side ports.

**Logical grid coordinate**  
The derived Zelda-like position of a map relative to its connected component.
It is not the node's browser canvas position.

**Exit overlay**  
The generated boundary opening, floor corridor, road, trigger, reserved area,
and arrival contributed by the World Map to a base map.

**Manual Player Spawn**  
An authored position for New Game, testing, or a future non-cardinal transport.
It is not a destination endpoint for normal side travel.

## Proposed world data

World format version 3 keeps free node positions but replaces arbitrary directed
Portal-to-Player-Spawn links with side-to-side connections:

```json
{
  "version": 3,
  "start": { "map": "wasteland", "spawn": "start" },
  "nodes": [
    { "map": "wasteland", "x": 255, "y": 173 },
    { "map": "two-rules-radial", "x": 510, "y": 80 }
  ],
  "connections": [
    {
      "id": "wasteland_north_pass",
      "a": { "map": "wasteland", "side": "north" },
      "b": { "map": "two-rules-radial", "side": "south" },
      "requirement": null
    }
  ]
}
```

Connection IDs are stable content IDs. Reordering JSON must not change them.
Validation rejects:

- unknown or disabled maps;
- duplicate connection IDs;
- invalid side names;
- connections from a map to itself;
- non-complementary side pairs;
- reuse of an occupied side port;
- cycles that imply contradictory logical coordinates; and
- different maps derived into the same logical grid cell.

Disabling a map removes its node and incident connections. It must not be
blocked merely because the map is connected.

## Deriving the logical grid

1. Assign the world-start map logical coordinate `(0, 0)`.
2. Traverse shared connections. North adds `(0, -1)`, east `(1, 0)`, south
   `(0, 1)`, and west `(-1, 0)`.
3. If a previously visited map receives a different coordinate, report the
   conflicting cycle and connection IDs.
4. If two different maps receive the same coordinate, report a logical-cell
   collision.
5. Repeat from an arbitrary origin for disconnected draft components. A
   compiled playable world should require all enabled release maps to be in the
   start component, while the editor may retain disconnected work in progress.

Logical coordinates are derived data and need not be saved. Moving nodes around
the browser canvas never changes them or breaks connections.

## Fixed exit geometry

For a generated region of width `W` and height `H`, the four boundary
centerlines are:

| Side | Boundary center | Inward direction |
| --- | --- | --- |
| North | `(floor(W / 2), 0)` | South |
| East | `(W - 1, floor(H / 2))` | West |
| South | `(floor(W / 2), H - 1)` | North |
| West | `(0, floor(H / 2))` | East |

Browser, compiler, host reference, and ROM must share one parameterized profile
for road width, shoulder, trigger inset/depth, arrival inset, and clearance.
"At least two pixels wide" means at least two map-generation cells, not two GBA
screen pixels. The final width must also satisfy vehicle-clearance validation.

## Exit overlay pipeline

The exit is applied after the base map has generated its terrain, towns, and
ordinary roads. For every connected side:

1. Build the map's four-bit exit mask from the saved world graph.
2. Derive its fixed midpoint boundary opening and inward approach geometry.
3. Reserve the approach and trigger area from later blocking placements.
4. Inspect whether the inward approach belongs to the primary driveable floor
   component.
5. If it is isolated, find the nearest cell in the primary open component using
   deterministic distance and tie-breaking rules.
6. Draw a simple deterministic line corridor from the approach to that cell,
   clearing walls to floor with a width of at least two generation cells.
7. Recompute or incrementally update floor connectivity.
8. Find the closest existing road section reachable through the now-connected
   floor and run the normal deterministic shortest-road connector. Multiple
   exit branches may merge.
9. Paint road through the connector, carved corridor, midpoint opening, and
   exact map boundary.
10. Clear or exclude walls, props, towns, encounters, radio collectibles, and
    other blockers from the full reserved area. Validate a car-width route from
    the boundary to the road network.

If generation cannot identify a primary open area or reachable road, it fails
with the map and side named rather than silently producing an unusable exit.

The terrain carve modifies only the compiled/previewed result. It does not
rewrite the source recipe. For a fixed recipe, seed, world graph, and overlay
algorithm version, the output must be identical.

## Preview, regeneration, and signatures

The map placement preview renders `base recipe + latest saved world overlay`.
It visibly distinguishes the boundary opening, transition trigger, carved floor
corridor, road connector, derived arrival, and any validation error.

Changing a seed reruns normal generation and then reapplies the same connected
sides. Changing a world connection invalidates the previews of both endpoint
maps. Map/world signatures used by runtime and saves must include the connected
side set and exit-overlay algorithm version so stale compiled maps are detected.

## Runtime transition

When the car enters a connected trigger:

1. Resolve the shared connection for the active map and side.
2. Preserve the current confirmation behavior initially.
3. Load the other map and select its opposite side.
4. Place the car at that side's derived arrival inset, facing inward.
5. Clear velocity and disarm the connection until the car leaves its trigger.

No Player Spawn lookup is involved, so ordinary travel cannot fall back to the
New Game position. Save/load records the current map and exact car state as it
does for other runtime state; emulator save states remain independent snapshots.

## Manual spawns after the change

Manual Player Spawns remain for:

- the New Game start;
- developer and test starts;
- future ferries, elevators, tunnels, interiors, or fast travel; and
- scripted relocation where cardinal continuity is not intended.

A special transport is a separate mechanic, not a fifth normal side exit.

## Browser World Map and atlas

The editable World Map remains a free-form graph canvas:

- nodes can be dragged anywhere without changing links or gameplay;
- every node displays four fixed side ports;
- the editor accepts only complementary port pairs;
- occupied ports cannot receive another connection;
- creation and removal update the one shared connection atomically;
- disabling a map removes its node and links without blocking the action; and
- undo/redo and save treat each connection edit as one operation.

A separate atlas mode derives logical grid coordinates and composes each map's
clean 128x128 wall/floor/road/town schematic on a pan-and-zoom canvas. It does
not use full textures. Connected midpoint roads should align across seams;
invalid components and future locked borders should be clearly marked.

## Future progression gates

Phase one stores `requirement: null` and implements no locks. Stable connection
IDs allow a later shared border to require an item, defeated boss, mission flag,
or repaired bridge without changing terrain generation.

A future locked connection should be symmetric, show a visible blocker inward
of both triggers, persist its unlock state, and participate in progression
reachability validation. A genuinely one-way transition should be a different
transport mechanic.

## Migration from world version 2

Current Portal IDs and positions do not reliably describe their physical side,
so migration must not guess:

1. Back up version-2 world and placement data.
2. Preserve each enabled map's free canvas position and the selected New Game
   start spawn.
3. Replace arbitrary endpoint handles with the four fixed side ports.
4. Let the author recreate the intended shared connections explicitly.
5. Preview both endpoint maps for each connection and adjust seeds if needed.
6. Save only after version-3 graph, terrain, road, and arrival validation passes.
7. Remove transition-only Portal rectangles and arrival spawns after the new
   data is confirmed.

Existing saves in enabled maps should continue through the current catalog
update handling. Obsolete route/transition state may be cleared when the world
signature changes, but player ownership and progression state must remain.

## Acceptance tests

Host/compiler tests should cover schema validation, every cardinal pairing,
logical-grid derivation, contradictory cycles, coordinate collisions,
deterministic overlay geometry, floor/road connectivity, clearance exclusions,
signature invalidation, and disabled-map pruning.

Browser tests should cover free dragging without topology changes, fixed port
connection editing, occupied/incompatible port rejection, atomic undo/redo,
preview invalidation, exact error reporting, and atlas stitching.

Headless mGBA tests should cover travel and return in all four directions,
correct opposite-side arrivals, no immediate bounce, no fallback to New Game
spawn, save/load across maps, visible road continuity, and maps with one through
four exits within ROM, RAM, and frame budgets. Subjective driving clearance and
visual quality still require emulator playtesting.

## Suggested implementation order

1. Add the version-3 side-port schema while retaining free node positions.
2. Implement validation and logical-grid derivation.
3. Remove ordinary Portal/arrival placement requirements from map recipes.
4. Generate fixed exit masks, triggers, arrivals, and reserved areas.
5. Add the two-cell terrain carve and closest reachable road connector.
6. Include overlay inputs and version in map/world signatures and host reference.
7. Change runtime travel to reciprocal side lookup.
8. Update the browser World Map to fixed side ports and shared connections.
9. Refresh map previews from world overlays and add the derived schematic atlas.
10. Add a guided migration, rebuild, and test every current map connection.
11. Implement progression requirements later as a separate focused feature.

## Decisions intentionally left for implementation/playtesting

- Exact road, shoulder, trigger, arrival, and clearance dimensions.
- Whether two generation cells are sufficient or the carve should always match
  the wider car-clearance profile.
- Whether normal edge travel retains confirmation or becomes automatic.
- How much visual seam the atlas shows between logically adjacent but
  intentionally unconnected draft regions.

## Implementation record

Implemented in seven vertical commits on 2026-09-28. The active world uses
`twin-cities.east <-> wasteland.west` and
`wasteland.north <-> two-rules-radial.south`. Version-2 transition placements
were intentionally discarded, as the current maps will be repaired and tuned
in the editor. The shared exit profile is `data/cardinal-exit-profile.json`.
