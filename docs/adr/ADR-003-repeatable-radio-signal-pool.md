# ADR-003: Use a repeatable session pool for radio signals

**Status:** Accepted | **Date:** 2026-09-27  
**Participants:** Marc Bielert, Codex

## Context

The first radio prototype placed three deterministic barrels in each compiled
map and saved a completion bit for every barrel. Playtesting showed that the
automatic receiver was pleasant to use, but permanently exhausting a small
fixed list did not support the intended long-term reason to roam Dustline's
large regions.

Signals must remain uncommon enough to reward scouting rather than provide an
easy scrap loop. Changing regions must not instantly refill a map, and the GBA
runtime must use bounded storage and work.

## Decision

We decided that each visited map owns a run-local pool of three to five active
signals. Positions are selected pseudo-randomly on reachable floor, away from
towns, portals, the player entry area, and other signals. The receiver acquires
the nearest active signal automatically only within a hard 1024-pixel range.

Destroying a barrel awards 12 scrap, removes that instance, and schedules a
replacement at a new valid position after a random three-to-five-minute delay.
Cooldown time advances with driving time across the run. Map changes retain the
pool and its cooldowns, so portal travel cannot be used to force an immediate
refill.

Signal positions and cooldowns are not written to the normal cartridge save.
Loading a normal save creates a fresh pool, while an emulator save state
naturally preserves the exact live pool and timers. The fitted receiver remains
normal persistent loadout data.

## Alternatives considered

### Permanent deterministic discoveries

Stable IDs and saved completion make every barrel a one-time authored find, but
the activity runs out and stops encouraging exploration.

### Immediate random replacement

Keeping the population full at all times is simple, but lets a player farm
nearby signals continuously and makes each discovery feel inconsequential.

### Refill whenever a map is entered

This keeps maps populated without timers, but makes portal hopping an obvious
exploit and discards useful in-session continuity.

### Persist every pool and cooldown in cartridge saves

This preserves exact regular-save state, but expands and couples the save format
to compiled map count for a lightweight repeatable activity. Emulator save
states already serve players who need exact suspension.

## Consequences

- (+) Radio salvage remains a repeatable reason to scout large maps.
- (+) Three to five active signals and delayed replacement bound reward rate.
- (+) Per-map run state prevents portal hopping from refilling the activity.
- (+) A hard range requires regional searching instead of guidance from anywhere.
- (+) The normal save format retains compatibility; its old radio completion
  words remain reserved.
- (-) Normal save/load rerolls positions and clears cooldowns, so it is not an
  exact activity-state restoration.
- (-) Random placement requires constraints and automated reachability checks.
- (Neutral) Emulator save states preserve exact source and cooldown state.
- (Neutral) The receiver remains passive and automatic as decided in ADR-002.
