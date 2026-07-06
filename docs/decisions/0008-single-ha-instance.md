# ADR-0008: Single Home Assistant instance instead of a Deck failover

**Status:** Accepted (2026-07-05), reversing an earlier dual-instance design

## Context

At one point the project ran two Home Assistant instances: a primary one and a secondary "failover" instance intended to take over if the primary went down. In practice, the failover was never actually wired to fail over automatically, and the dual-instance setup (via a `remote_homeassistant`-style mirroring integration) was found to be the root cause of a whole class of bugs: duplicate entity IDs shadowing real ones, broken STT/TTS, a shared "which instance is active" flag getting stuck and silently disabling all push automations to this device (see [`docs/troubleshooting.md`](../troubleshooting.md)).

## Decision

Home Assistant runs as a single instance. The secondary instance was stopped and disabled at boot (kept on disk, reversible, but not started). The device on the second host that used to run it is still used for other scripts/containers — just not as an HA node.

## Consequences

- Eliminates the entire class of duplicate-entity and stuck-flag bugs described in `docs/troubleshooting.md` — there is no longer a second instance to duplicate anything or to disagree about which one is "active."
- There is now no automatic failover if the single HA instance goes down. This was an explicit tradeoff: the documented failover had never actually worked as designed, so removing it didn't remove real redundancy, only the appearance of it.
- If a future contributor is tempted to reintroduce a second HA instance (e.g. for genuine high availability), read the troubleshooting entries above first and plan for the entity-ID-collision problem explicitly rather than re-discovering it.
