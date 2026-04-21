# Phase E — CRDT-backed Canonical (SCOUTING)

**Type:** Scouting doc. Expand to full plan if and when a concrete sync-conflict pain signal materializes (not before).

**Parent:** Markoff Phase C3 — [`MarkoffDocument` content-authoritative spec](../../../libs/markoff-family/docs/specs/2026-04-20-phase-c3-markoff-document-content-authoritative.md) §11 "Phase-E hedge".

**Not on the Obsidian-parity roadmap.** Sits alongside clusters (post-parity product work). Not cluster-numbered; Phase E is a **product-level** phase that might happen or might not, depending on whether multi-device sync-conflict pain becomes a real thing for Corbomite users.

---

## Motivation

Obsidian charges $8/month for its Sync service. Most users roll their own sync via Syncthing / Dropbox / iCloud / NAS / git, and occasionally lose work to `.sync-conflict` files when two devices edit the same note offline.

A conflict-free-by-construction markdown canonical — where two clients' edits to the same note reconcile without human intervention — is the kind of feature that forms a real wedge against the incumbent's recurring-revenue product. More importantly, it's the kind of feature that's *hard* to add later without re-architecting canonical storage, and *easy* to add later if canonical is shaped to accept it.

Phase C3 ships the shape. Phase E ships the swap.

## Technical path

`~/dev/collabtext/` is a plain-text offline-first CRDT engine (C++20, Qt-free core). Plain-text-only, which is exactly what Markoff's canonical is. The engine is production-ready; editor integration is specified-not-implemented. Being the first integration consumer means the integration shape is ours to set.

### The C3 hedge carries the weight

Phase C3 introduced two surfaces specifically to leave this door open:

1. **`Markoff::CanonicalBuffer` interface** — `MarkoffDocument::Private` holds a `std::unique_ptr<CanonicalBuffer>`. C3 ships one concrete: `InMemoryCanonicalBuffer` (backed by `QString` + anchor table).
2. **`Markoff::CursorPosition` opaque handle** — anchor-backed cursor tracking through `MarkoffDocument::trackCursor` / `resolveCursor`.

Phase E adds a second concrete: `CrdtCanonicalBuffer`, backed by `collabtext::Buffer`. Same interface; same signal surface from `MarkoffDocument`; no leaf sees the difference.

### What changes internally

- `InMemoryCanonicalBuffer::applyDelta(offset, removedLength, inserted)` becomes `CrdtCanonicalBuffer::applyDelta(offset, removedLength, inserted)` — translated to `collabtext::Buffer::apply_local_edit(...)`.
- Anchor creation/resolution translates to `collabtext` anchor primitives (anchors are a first-class CRDT concept; the implementation is simpler than the `InMemoryCanonicalBuffer` hand-rolled offset-adjusting table).
- Remote edits (from a peer's sync merge) arrive through a new `CrdtCanonicalBuffer::applyRemoteEdit(...)` path that synthesizes `contentsChanged` emissions on the main thread — leaves get normal delta signals regardless of local-vs-remote provenance. The undo stack does *not* record remote edits (they're not user-intent edits by this user).
- A new `MarkoffDocument::peerReconciled()` signal announces remote-edit arrival, analogous to `documentReloaded` but delta-shaped rather than wholesale.

### What changes externally (Corbomite side, Phase E scope)

- Vault's save path writes not just the markdown bytes but also a sidecar `.crdt` state blob (or similar — storage format is a Phase-E-level decision, see non-commits below).
- Vault's watcher path learns to distinguish "disk changed because a peer wrote" (apply as CRDT merge) from "disk changed because an external editor wrote" (current external-reload path, `Origin::ExternalReloadClean|Resolved`).
- A small status-line widget shows sync health (unsynced edits, last peer reconcile, conflict markers if somehow they survive).

## Explicit non-commits

Phase-E scouting commits to **none** of these; each is a separate decision at Phase-E expansion time:

- **Storage format.** Full-CRDT-folder vs. markdown-plus-sidecar vs. CRDT-in-memory-only are three different answers with different user-visible behavior. Don't preempt.
- **Real-time collaborative editing.** Multi-user live typing (Google-Docs-style) is an additional product-line decision on top of Phase E; nothing about the C3 hedge or Phase E forces a network stack into the roadmap.
- **Sync transport.** File-sync (Syncthing, Dropbox) vs. custom-server vs. P2P are orthogonal. CRDT state can live on any transport.
- **A Corbomite-branded sync service.** Recurring-revenue product decision, not a technical decision. Phase E is about *enabling* users' existing sync tools to work conflict-free; commercializing is a separate conversation.

## Gating

Phase E expansion (from scouting doc to spec + plan) should happen only when all of the following are true:

1. **Markoff Phase C is done** (all seven work-units closed on the Phase C status board).
2. **`CanonicalBuffer` abstraction has held up for at least 6 months** of normal development (no in-phase-flight contortions that reveal interface design flaws).
3. **A concrete sync-conflict pain signal has materialized** — users reporting `.sync-conflict` files, lost work from Syncthing merges, or explicit asks for "conflict-free sync." Without this, Phase E is YAGNI; the abstraction alone suffices.
4. **A collabtext integration example exists** — either written by us or by the collabtext maintainer. Going first without a reference integration is high risk.

## What this SCOUTING doc does NOT decide

- Whether Phase E happens at all. The C3 hedge is justified on clean-abstraction grounds alone (per C3 spec §4.2 "Honest cost note").
- When Phase E happens. No date; no cluster slot in the roadmap.
- Who implements Phase E. Could be the Corbomite agent, the Markoff agent, a collabtext maintainer, or a contributor attracted by the feature.
- What Phase E is called. The "Phase E" label is provisional; if there are intermediate Phase D work items (there currently are none), renumber.

## Pointers

- Markoff Phase C3 spec §11 (the original hedge): `libs/markoff-family/docs/specs/2026-04-20-phase-c3-markoff-document-content-authoritative.md`.
- `Markoff::CanonicalBuffer` interface (when it lands): `libs/markoff-family/libs/markoff-core/include/markoff/CanonicalBuffer.h`.
- `Markoff::CursorPosition` opaque handle (when it lands): `libs/markoff-family/libs/markoff-core/include/markoff/CursorPosition.h`.
- collabtext engine: `~/dev/collabtext/` (local checkout).
- Related scouting: Cluster V.2 debt cleanup, Cluster W canvas/graph, Cluster O query layer — all post-parity.
