# ADR-002: Fit the radio receiver in the top special slot

**Status:** Accepted | **Date:** 2026-09-27  
**Participants:** Marc Bielert, Codex

## Context

The directional radio system described by
[ADR-001](ADR-001-radio-signal-discovery.md) could be built into every car, made
an automatically unlocked upgrade, or treated as vehicle equipment. A universal
receiver would add exploration guidance without requiring a player choice, but
it would also become permanent HUD functionality and weaken loadout identity.

Dustline already has a top special slot used by missiles and traps. Putting the
receiver in this slot integrates discovery with the garage, inventory, save
data, and existing loadout choices. It also creates a direct tradeoff between
finding hidden rewards and carrying an active combat special.

## Decision

We decided that the radio-signal receiver is a special item fitted in the top
slot. It has its own icon in the garage's top-slot equipment selection.

The directional proximity overlay is active only while the receiver is fitted.
Removing or replacing it immediately disables signal tracking and removes the
overlay. Missiles and traps remain mutually exclusive alternatives in the same
slot.

The receiver is passive. It consumes no battery energy, and pressing `L`
performs no action while it is fitted. Playtesting confirmed that automatic
tracking avoids an annoying hold-to-scan action. The occupied special slot is
its gameplay cost. A future version may use `L` to cycle signals if overlapping
discoveries make manual selection useful.

Signal-source objects exist independently of the receiver. A player without the
receiver can still find and shoot a barrel by chance, but receives no directional
guidance. This makes the item an exploration tool rather than a key that makes
world objects exist.

The equipped receiver is part of the saved vehicle loadout and follows the same
persistence rules as fitted missiles and traps.

## Alternatives considered

### Receiver built into every car

This guarantees that all players see the discovery mechanic, but removes the
loadout choice and makes the overlay a permanent part of driving.

### Separate utility slot

This allows the radio and a combat special at the same time, but introduces a
new equipment category, UI, controls, and save data for a single prototype item.
It can be reconsidered if several utility items are eventually designed.

### Consumable scanner

A limited-use scan gives scrap another purpose, but encourages menu use and does
not create a persistent vehicle identity.

### Radio required to reveal the world object

This provides a hard equipment gate, but makes accidental discovery impossible
and makes barrels appear to exist only because a UI item is installed.

### Active receiver operated with `L`

An active pulse or target-cycle control could add agency, but hold-to-scan made
the otherwise continuous driving/search loop needlessly laborious. Automatic
tracking keeps the player's attention on scouting and interpreting the signal.

## Consequences

- (+) Exploration capability becomes a meaningful garage loadout decision.
- (+) Existing top-slot selection, inventory, and save concepts can be reused.
- (+) The unique icon makes the receiver's installed state explicit.
- (+) Players can still discover barrels without the receiver.
- (+) No additional utility slot or driving input is required.
- (-) Equipping the radio means giving up missiles or traps.
- (-) `L` has no immediate action while the passive receiver is fitted.
- (-) Players who never equip it may miss most hidden discoveries.
- (Neutral) The prototype has no ongoing energy cost; the slot tradeoff provides
  its initial balance.
- (Neutral) A separate utility slot remains an option if the game later gains a
  broader family of non-weapon vehicle equipment.
