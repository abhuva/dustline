# ADR-004: Use deterministic regional inventories for town shops

**Status:** Accepted | **Date:** 2026-09-28
**Participants:** Marc Bielert, Codex

## Context

The garage shop previously exposed the same complete nine-item catalog in every
town. That made purchases easy to understand, but removed a reason to visit
different towns and regions. As the item catalog grows, displaying everything
everywhere will also make the shop crowded and prevent geography from guiding
progression.

The desired progression is spatial rather than level-scaled. Reaching another
region should reveal different or stronger equipment. A player who visits a
shop early must be able to remember what it sells and return later when they can
afford it. Player level, elapsed time, repeated visits, loading, and ordinary
runtime randomness must not make items silently appear or disappear.

Purchased equipment is globally owned. Once purchased, it should no longer be
offered by any shop. Removing an owned item must not cause an unseen replacement
to enter that town's inventory, because the visible result should be exactly the
town's known inventory minus already-owned items.

## Decision

We use a layered, deterministic shop system with three authoring levels:

1. Each item has a stable ID, an immutable save ID, a positive tier, an item
   family (`upgrade`, `front`, `side`, or `top`), and a region policy. Region
   policies support all regions, only a named set, or every region except a
   named set.
2. Each map recipe has a regional shop profile. It defines the allowed tier
   floor, a deterministic range of town tier caps, base stock size, and the
   preferred mix of item families.
3. Individual towns may override the regional defaults with a tier offset,
   stock-size adjustment, or different family weights.

The build will resolve these layers into one fixed base inventory for every
compiled town. Resolution uses only stable content data: item IDs, map ID,
recipe seed, town index, and an explicit shop-generation version. The resolved
inventories are compiled into the ROM. They are not generated from the live
gameplay RNG and do not need to be stored in a save game.

At runtime a shop displays its compiled base inventory in its fixed order after
filtering out globally owned items. It does not refill holes, select substitutes,
scale against a player level, rotate stock, or reroll on entry. If every base
item is owned, the shop displays a sold-out state.

Tier is placement metadata, not a player requirement. If a player reaches a
high-tier region early and can pay the price, the item may be bought. Prices and
world access remain the gameplay gates.

The current direct-purchase behavior remains. Although the equipment is
occasionally described conversationally as a blueprint, buying it grants
ownership immediately; this decision does not restore crafting or 3D printing.

## Alternatives considered

### Keep the complete catalog in every town

This is simple and guarantees access, but gives towns no commercial identity,
weakens exploration, and does not scale with a larger item catalog.

### Unlock stock from player level or general progress

This creates a familiar progression curve, but makes shops change behind the
player and encourages revisiting every known town after each threshold. It also
conflicts with the goal that better equipment comes from reaching new places.

### Reroll stock per visit, day, load, or new game

Random rotation creates variety, but makes acquisition unreliable, encourages
save scumming and repetitive travel, and prevents the player from learning
where an item is sold.

### Author every town's complete inventory manually

This provides maximum control and perfect stability, but duplicates data and
becomes costly as maps and items grow. Regional defaults with optional town
overrides preserve control without requiring every stock list to be maintained
by hand.

### Generate inventories at runtime and save the results

This can also remain stable, but consumes save space, complicates migrations,
and makes the result depend on runtime generation order. Build-time resolution
is easier to validate and guarantees identical stock for a given ROM.

## Consequences

- (+) Towns and regions gain distinct commercial identities and reasons to visit.
- (+) A shop is learnable: its stock changes only when the player buys an item
  or installs a different ROM build.
- (+) Higher-tier progression is driven by world access rather than player level.
- (+) Region policies support global, region-exclusive, and region-excluded items.
- (+) Build-time resolution keeps runtime memory, RNG, and save requirements small.
- (+) Optional town overrides allow authored specialty shops without duplicating
  every regional rule.
- (-) Item, region, and town metadata require compiler and editor validation.
- (-) Catalog or recipe edits may intentionally change inventories in a newer
  ROM build, so automated inventory snapshots are needed.
- (-) Shops can become sparse or sold out as ownership grows; the UI must handle
  fewer than nine entries cleanly.
- (-) Poorly chosen tier windows or family weights can leave a town without
  enough eligible candidates; the compiler must report this rather than silently
  invent stock.
- (Neutral) The current nine-cell shop remains a presentation limit for the
  initial implementation, not a fundamental item-catalog limit.
- (Neutral) Ownership remains global and persistent; inventories themselves do
  not become save data.

## Related documents

- [Shop inventory and regional progression design](../shop-inventory-progression.md)
- [Dustline world activities](../world-activities.md)
