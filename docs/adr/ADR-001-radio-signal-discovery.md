# ADR-001: Use directional proximity signals for hidden discoveries

**Status:** Accepted | **Date:** 2026-09-27  
**Participants:** Marc Bielert, Codex

## Context

Dustline's regions are large and need reasons to explore beyond explicit towns,
portals, missions, and races. Placing every discovery directly on the minimap
would make navigation clear, but it would reduce exploration to following exact
icons. Audio-only searching would fit the radio theme but would be inaccessible
in muted play and difficult to tune for noisy environments.

The existing fixed elevated view keeps the player car near the center of the
screen and preserves world orientation. This makes a compact directional UI
near the car practical. The GBA target also favors a small sprite-based display
over text, transparency effects, or a large additional HUD.

## Decision

We decided to use a proximity-based radio indicator for hidden discoveries. The
indicator appears near the player car, points toward one tracked source in
one of eight world-relative directions, and displays one, two, or three chevrons
as signal strength increases.

Radio discoveries do not receive exact minimap markers. At close range their
world object becomes visible and the player finishes the search by interacting
with that object. The first prototype object is a static barrel
collected by shooting it.

Explicit mission, race, and navigation objectives will retain their existing
map markers. The radio indicator is a discovery mechanic rather than a universal
replacement for goal navigation.

The original prototype gave each source a stable ID and persisted completion.
That source-lifetime decision was later superseded by
[ADR-003](ADR-003-repeatable-radio-signal-pool.md). Only nearby runtime objects
and the currently tracked indicator need to be instantiated.

The indicator is conditional on a radio-signal receiver fitted in the vehicle's
top special slot. The equipment decision and its consequences are recorded in
[ADR-002](ADR-002-radio-top-slot-equipment.md).

Detailed prototype behavior and the broader activity catalog are recorded in
[Dustline world activities](../world-activities.md).

## Alternatives considered

### Exact minimap marker

This is highly readable and reuses the existing goal UI, but removes the search
and makes the activity feel like another waypoint.

### Approximate circle on the map

This preserves some uncertainty but requires opening or watching the map and
does not directly enrich moment-to-moment driving.

### Screen-edge compass arrow

This is familiar and readable, but visually resembles a mandatory objective.
Keeping the indicator close to the car better connects it to the radio-equipped
vehicle and makes distance changes easy to notice.

### Audio-only signal

Changing beep frequency could communicate distance elegantly, but it cannot be
the sole channel for muted play, hearing accessibility, or loud environments.
Audio may supplement the visual indicator later.

### Full free-direction pointer

A smoothly rotated pointer would be more precise, but the additional precision
reduces the final search and is less economical in GBA sprite art. Eight
directions provide enough guidance while retaining uncertainty.

## Consequences

- (+) Exploration remains more active than following exact map icons.
- (+) Direction and proximity are readable without text or opening a menu.
- (+) Eight directions and three strengths have a small, predictable art and
  runtime cost.
- (+) The same discovery system can later support wrecks, distress calls,
  ambushes, rare vehicles, and moving targets.
- (Superseded by ADR-003) Stable activity IDs allowed discoveries to integrate
  with existing saves, but made a finite exploration activity rather than a
  repeatable reason to scout.
- (-) Multiple nearby sources require a locking rule to avoid indicator churn.
- (-) Distance thresholds, placement offset, animation, and final-search radius
  require playtesting at normal driving speed.
- (-) An always-on overlay can obscure combat or terrain if its priority and
  placement are not carefully constrained.
- (-) The system needs separate visual language from race and mission markers.
- (Neutral) Explicit objectives continue to use conventional map navigation.
- (Neutral) The initial barrel is deliberately simple and validates discovery,
  targeting, destruction, reward, and replacement before larger encounters are
  built.
