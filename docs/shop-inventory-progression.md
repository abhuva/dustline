# Shop inventory and regional progression

**Status:** Implemented
**Last updated:** 2026-09-28
**Related decision:** [ADR-004](adr/ADR-004-deterministic-regional-shop-inventories.md)

## Player-facing goal

Different towns should be worth discovering because they sell different things.
The player can arrive before they can afford an item, remember its location, and
return later with confidence that it will still be there.

Progression comes from reaching new regions. It does not come from shops watching
the player's level and updating themselves. A level-1 and a hypothetical
level-9000 player see the same base inventory in the same town. The only runtime
difference is that globally owned items are removed from the list.

## Required invariants

- A town has one fixed base inventory for a given ROM build.
- Entering, leaving, loading, waiting, or changing player statistics never rerolls it.
- Player level is not an input to availability or stock selection.
- Purchased items disappear from every shop immediately.
- Removing an owned item never backfills another item into the visible inventory.
- Reaching a region and having enough currency are sufficient to buy its stock;
  tier is not an additional lock.
- The same item may deliberately be stocked by multiple towns.
- A shop with no remaining items shows `SOLD OUT` instead of an invalid cursor.

In short, if a town's base inventory is `[A, B, C, D]` and the player owns `B`
and `D`, the town displays `[A, C]`. It must not pull `E` and `F` from the pool.

## Terminology

**Catalog item**  
One globally defined purchase. It has a stable content ID and save ID.

**Family**  
The shop-placement group: `upgrade`, `front`, `side`, or `top`. The family means
the garage mount an item competes for, not the direction in which it fires. For
example, the forward-firing side weapon remains in the `side` family.

**Region policy**  
A hard location filter on an item: all regions, only named regions, or every
region except named regions.

**Tier**  
A positive authoring value used when building town inventories. It is not shown
as player level and does not gate a purchase after the item is present.

**Regional shop profile**  
Map-level defaults controlling tier range, stock size, and family mix.

**Town modifier**  
An optional adjustment for one stable town index within a map.

**Base inventory**  
The fixed ordered item IDs compiled for a town before owned items are removed.

## Source data

The item catalog lives in shared source data so the
map compiler, editor, host tests, and ROM generator validate the same rules. The
canonical path is `data/shop-items.json`.

An item record should contain data equivalent to:

```json
{
  "id": "long_sniper",
  "saveId": 4,
  "name": "LONG SNIPER",
  "detail": "SLOW / RANGE 520",
  "credits": 320,
  "scrap": 18,
  "kind": "weapon",
  "weapon": "sniper",
  "family": "front",
  "tier": 2,
  "regions": {
    "mode": "only",
    "ids": ["wasteland", "twin-cities"]
  }
}
```

Region policy modes have exact meanings:

- `all`: allowed in every enabled region; `ids` must be absent or empty.
- `only`: allowed only in the listed stable map IDs.
- `except`: allowed in every enabled region except the listed stable map IDs.

Unknown map IDs, duplicate item IDs, duplicate save IDs, non-positive tiers,
and incompatible weapon/family combinations are build errors. Save IDs are
append-only and must never be reused, even after an item is retired. Existing
items reserve their current ownership positions 0 through 8.

## Map recipe profile

Shop configuration belongs to map recipe metadata alongside art and encounter
profiles. It should not be a graph node because it does not transform terrain.
Recipe version 7 accepts data equivalent to:

```json
{
  "shopProfile": {
    "tierFloor": 1,
    "townTierRange": [1, 2],
    "stockSize": 6,
    "mixWeights": {
      "upgrade": 1,
      "front": 3,
      "side": 2,
      "top": 2
    },
    "townModifiers": [
      {
        "town": 0,
        "tierOffset": 0,
        "stockDelta": 1,
        "mixWeights": { "front": 5, "side": 1, "top": 1 }
      },
      {
        "town": 4,
        "tierOffset": 1,
        "mixWeights": { "front": 1, "side": 1, "top": 5 }
      }
    ]
  }
}
```

Recommended semantics:

- Each town receives a deterministic tier cap within `townTierRange`.
- `tierOffset` adjusts that town's cap.
- An item is tier-eligible when `tierFloor <= item.tier <= town tier cap`.
- `stockSize + stockDelta` is clamped to the initial UI capacity of nine.
- Town weights replace only the supplied regional family weights; omitted
  families keep their regional values.
- Zero weight prevents the generator from deliberately choosing that family,
  although the fallback rules below may still be configured to fill shortages.

These schema names and semantics are implemented. The Map Workshop exposes a
Shop Profile dialog with an exact preview table for all six towns.

## Deterministic stock resolution

The build resolves inventories in this order:

1. Start with all catalog items.
2. Apply each item's region policy to the current map ID.
3. Apply the map tier floor and the town's deterministic tier cap.
4. Derive the town's stock count and family weights from the regional profile
   plus its optional modifier.
5. Build a deterministic family schedule from those weights.
6. Rank candidates within each family using a stable hash of shop-generation
   version, map ID, recipe seed, town index, and item ID.
7. Select without replacement until the stock count is met.
8. If a requested family is exhausted, use a documented deterministic fallback
   from the remaining eligible families. If the complete pool is too small,
   fail the build with the map and town identified.
9. Emit the ordered base inventory into generated ROM data.

This algorithm must not consume the procedural world's runtime RNG. The explicit
shop-generation version allows a deliberate future algorithm change without
pretending inventories remained identical.

Build-time resolution is preferable to recalculating in the ROM: it gives the
editor an exact preview, costs no driving-time CPU, needs no save data, and lets
host tests snapshot every town's stock.

## Runtime and UI behavior

The town shop separates these concepts:

- `base_stock[]` contains catalog/save IDs for the current map and town.
- `visible_stock[]` is rebuilt by preserving base order and excluding owned IDs.
- Grid position indexes `visible_stock`, never the global catalog directly.
- The icon, price, information panel, and purchase action resolve through the
  selected visible item ID.
- After purchase, rebuild `visible_stock` immediately and clamp selection to the
  next valid cell. Do not replace the purchase with another pool item.
- Fewer than nine entries are packed row-major with no selectable empty cells.
- No entries produces a clear `SOLD OUT` message; B still returns normally.
- Tier may remain hidden authoring metadata. It does not need a player-facing
  number unless later playtesting finds that useful.

The first implementation can retain the 3×3 maximum. If shops later need more
than nine base items, pagination should be added explicitly; the global catalog
must not inherit a nine-item limit from the current UI.

## Ownership and saves

Ownership remains global across all maps and towns. Shops do not persist stock,
discovery time, or visit state.

The prototype now stores immutable save IDs in a generated ownership bitset sized
for the reserved ID range. Save format version 2 carries four ownership words,
while its version-1 migration preserves the legacy nine bits. The format:

- migrate the existing nine ownership bits without changing their meaning;
- preserve old valid saves;
- never interpret catalog array order as persistent identity;
- reserve removed IDs instead of reusing them;
- remain bounded by the cartridge payload budget, not by the 3×3 UI.

## Map Workshop and validation

The Shop Profile editor shows:

- regional tier floor and town tier-cap range;
- base stock size;
- upgrade/front/side/top weights;
- optional per-town overrides using the same stable town indices and names as
  the Select-map view;
- an exact generated inventory preview for each town;
- warnings for empty families and errors for undersized eligible pools.

Changing `includeInGame` or the world graph does not directly change item rules.
Region policies reference stable map IDs. Disabled maps may keep valid shop
profiles as drafts, while only enabled maps receive compiled runtime stock.

## Initial balancing approach

The first content pass should be conservative:

- Keep the starting region focused on tier-1 essentials and a few tier-2 goals.
- Put at least one useful but non-mandatory specialty in each reachable region.
- Do not place all top, side, or front choices in a single town unless that town
  is intentionally a specialist.
- Use `except` or a raised tier floor to remove obsolete low-tier stock from
  later regions.
- Allow some overlap so a missed town does not permanently block core equipment.
- Keep the radio obtainable early enough that it can support exploration.
- Treat price as the reason to return later; do not add hidden level checks.

The initial item tiers, region policies, weights, and prices are committed as
balancing data and remain reviewable through generated previews.

## Acceptance tests for implementation

Host-side tests verify:

- all item IDs and save IDs are unique and region references exist;
- the same inputs produce byte-identical town inventories;
- every enabled map and town has a valid stock count and no duplicate item;
- tier, region, and family rules are respected;
- unrelated runtime RNG consumption cannot change stock;
- catalog order changes do not change ownership identity;
- the generated inventory snapshot changes only when relevant content or the
  explicit shop-generation version changes.

Headless mGBA tests verify:

- two towns can expose different base inventories;
- revisiting, saving/loading, and changing player statistics preserve stock;
- buying an item removes it immediately and from every other town that stocked it;
- purchase removal does not backfill a new item;
- selection remains valid as the list shrinks;
- a fully purchased shop displays `SOLD OUT` and remains safely escapable;
- old saves retain all previously purchased equipment.

## Implementation record

The feature landed as six vertical commits:

1. Shared catalog, stable IDs, validation, and generated C++ data.
2. Stable-ID ownership bitsets plus version-2 saves and version-1 migration.
3. Recipe-version-7 profiles and deterministic per-town compiled inventories.
4. Packed runtime inventory filtering, global removal, and `SOLD OUT` handling.
5. Map Workshop authoring and exact six-town previews through the canonical resolver.
6. Regional balancing, host snapshots, headless-ROM acceptance coverage, and docs.
