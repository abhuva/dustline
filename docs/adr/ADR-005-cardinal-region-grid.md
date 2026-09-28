# ADR-005: Derive cardinal region exits from the world graph

**Status:** Accepted and implemented | **Date:** 2026-09-28
**Participants:** Marc Bielert, Codex

## Context

Dustline currently models normal region travel with freely positioned Portal
triggers connected to separately positioned Player Spawns. This makes map
authoring depend on transition setup and requires the author to align triggers,
arrivals, open terrain, and roads manually. It also permits one-sided links,
misleading endpoint names, and transitions that accidentally use a map's start
position.

The desired workflow is map-first. A map recipe should describe the region
without knowing which of its four borders will eventually connect to other
regions. The World Map should establish topology later. Once a connection is
made, each affected map preview should show the resulting opening and road
without requiring Portal or arrival placement.

The node canvas is useful when freely arranged for editing. That presentation
layout does not need to be the same thing as the Zelda-like logical layout used
by the game and a future atlas.

## Decision

Normal map recipes do not contain authored Portal or arrival nodes. Every World
Map node always exposes four fixed side ports: north, east, south, and west. A
side can have at most one normal connection.

World Map nodes retain freely draggable canvas coordinates. These coordinates
are presentation data only and never define gameplay adjacency. Connections may
only pair complementary sides: north with south or east with west. A connection
is one reciprocal object, created and removed atomically; it is not two directed
edges.

The game derives a logical integer grid from those connections. Starting from
the world-start map at `(0, 0)`, crossing a side changes the logical coordinate
by one cardinal unit. Cycles must derive the same coordinate by every route, and
two different maps cannot occupy the same derived coordinate. This supplies a
stable atlas layout without forcing the editable graph canvas to snap to a grid.

Each connected side applies a deterministic exit overlay after normal map
generation:

- the exit center is fixed at the midpoint of the connected edge;
- a reserved, wall-free approach reaches the exact map boundary;
- road paint continues to that boundary;
- if the approach is isolated from the primary driveable area, generation finds
  the nearest primary open space and carves a deterministic simple corridor at
  least two generation cells wide;
- after floor connectivity exists, the normal deterministic road-connection
  algorithm joins the exit to the closest reachable road section;
- transition trigger and arrival position are derived from the same exit
  profile; and
- walls, props, towns, encounters, and collectibles cannot block the reserved
  approach.

The final playable map is therefore `base recipe + world-graph exit overlay`.
The base recipe and seed remain unchanged. Regenerating or changing a seed
recomputes both the base map and its current overlay, so the author can inspect
connections in the map preview and try another seed without manually rebuilding
exits.

Manual Player Spawns remain for New Game, developer testing, and future special
transport such as interiors or fast travel. Normal cardinal travel derives its
arrival from the destination's opposite side and does not use a named spawn.

Connections have stable IDs and reserve optional requirement metadata. The
initial implementation leaves requirements empty. Future boss, item, and story
gates belong to the shared connection and do not alter its terrain geometry.

## Alternatives considered

### Keep arbitrary Portal rectangles and Player Spawn destinations

This preserves maximum local control, but retains the alignment, validation,
wrong-spawn, and repeated authoring work that motivated the change.

### Author integer grid coordinates directly

Snapping the editor canvas to gameplay coordinates makes a stitched atlas easy,
but makes the working graph less convenient to arrange. Cardinal connections
already contain enough information to derive the logical grid, so editor layout
and gameplay topology should remain separate.

### Fix Portals to edge midpoints but keep directed graph links

This improves geometry while still allowing missing return paths, mismatched
opposite sides, and two independently maintained halves of one border.

### Require authors to carve every exit and road manually

This gives fine control but couples region design to topology and makes changing
connections or seeds expensive. A deterministic overlay provides a useful
default while seed selection retains broad layout control.

### Stitch full rendered maps into the world atlas

Full textures add generation cost and visual noise at world scale. The clean
128x128 wall, floor, road, and town schematics are more useful for planning.

## Consequences

- (+) Maps can be authored and regenerated without first deciding world exits.
- (+) The World Map becomes the single source of truth for normal topology.
- (+) Free canvas arrangement remains available without affecting gameplay.
- (+) Every normal transition is reciprocal and uses a derived safe arrival.
- (+) Map previews show the real generated corridor and road after linking.
- (+) The same graph can produce a logical region atlas and later progression
  sections.
- (-) The generator needs a deterministic terrain-carving and road-overlay pass.
- (-) Contradictory graph cycles and logical-cell collisions need clear editor
  validation.
- (-) Existing arbitrary Portal and arrival data needs an explicit migration;
  current endpoint names and positions are not reliable enough to infer sides.
- (-) Normal exits lose arbitrary position, multiplicity per side, and one-way
  behavior by design.
- (Neutral) Moving a node on the free-form canvas has no gameplay effect.
- (Neutral) The current travel confirmation can remain initially; changing that
  interaction is independent of this topology decision.

## Related documents

- [Cardinal world graph and derived edge exits](../cardinal-world-grid.md)
- [Dustline world activities](../world-activities.md)
