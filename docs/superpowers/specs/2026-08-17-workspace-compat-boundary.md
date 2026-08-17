# Workspace compat-boundary doctrine

**Date:** 2026-08-17 · **Status:** Accepted — Clinton sign-off 2026-08-17 · **Track:** Cluster L, Phase L0
**Plan:** [`../plans/2026-08-17-cluster-l-workspace-stabilization.md`](../plans/2026-08-17-cluster-l-workspace-stabilization.md)

---

## 1. The doctrine

**Compatibility with Obsidian lives at the file-format boundary, not in the
object model.** Concretely:

- **Schema-at-rest fidelity is mandatory.** `.obsidian/workspace.json` must
  round-trip Obsidian's exact schema: every key Obsidian writes, including
  keys Corbomite doesn't understand or that a future Obsidian version
  introduces, survives a Corbomite load→save cycle byte-for-byte (modulo a
  short explicit allow-list of intentional rewrites — see §2 and Phase L2's
  golden test). This is the actual interop contract: a user who opens the
  same vault in both apps must not lose data or have either app corrupt the
  other's state.
- **In-memory shape mimicry is opt-in per demonstrated need, not a
  default.** `Workspace`/`WorkspaceLeaf`/`WorkspaceSerializer` do not need to
  reproduce Obsidian's JS runtime object graph, teardown order, or event
  timing. `WorkspaceRoot`/`WorkspaceContainer`/`WorkspaceSidedock`/
  `WorkspaceFloating`/`WorkspaceWindow` (Cluster L finding C1) are exactly
  this failure mode: shells kept so Obsidian-shaped code "compiles" against
  them, with `leftSplit()`/`rightSplit()` returning literal `nullptr`
  (`Workspace.h:256-257`). A caller that treats that nullptr as "there is no
  left split" crashes. Compiling against an Obsidian-shaped API is not the
  same as being compatible with Obsidian — the only thing a plugin or a
  human ever actually observes is the file on disk (and, for plugins, the
  documented Obsidian JS API surface, which Corbomite doesn't implement
  today and isn't in scope here).
- **The serializer is the only compatibility-enforcing layer.** Everything
  behind `WorkspaceSerializer::parse`/`serialize` — leaf lifetime, KDDW
  wiring, focus routing, undo — is free to be idiomatic Qt/KDE. Cluster L's
  crash cluster (Phase L1: A1–A4) exists because teardown code was written
  to *feel* like it was preserving Obsidian's object lifetime instead of
  just being correct Qt/KDE lifetime management. This doctrine explicitly
  licenses fixing that without worrying about "but does Obsidian do it this
  way" — Obsidian's runtime is not part of the contract.

**Practical test for any future "should this mimic Obsidian" question:**
does an Obsidian-authored `workspace.json` loaded into Corbomite, edited,
and saved still open correctly in Obsidian? If yes, the in-memory
implementation is free to diverge. If a plugin API surface is ever
implemented (out of scope for Cluster L), *that* is a second, separate
compat boundary with its own doctrine question — not covered here.

---

## 2. The B2 decision: what belongs in `workspace.json`

### Current behavior (verified)

`SessionManager::doSave()` (`src/app/SessionManager.cpp:246-278`) composes
the on-disk root from `m_unknownRoot` (Obsidian keys round-tripped
unchanged) plus three Corbomite-written keys:

- `main` / `active` — legitimate Obsidian schema keys (the layout tree,
  active leaf id).
- `_corbomite` — a Corbomite-private object: base64 `windowGeometry`/
  `windowState` (`saveWindowGeometry`, line 122), sidebar visibility/widths
  (`saveSidebarState`, line 132), expanded-folder list (`saveExpandedFolders`,
  line 156), and per-plugin session blobs (`setPluginSessionState`,
  line 164).
- `left-ribbon` — Corbomite's left-ribbon layout state
  (`setLeftRibbonState`, line 178).

Neither `_corbomite` nor `left-ribbon` exists in Obsidian's schema. They are
written into `.obsidian/workspace.json` today (finding B2 in the cluster
plan), a file **Obsidian rewrites wholesale on every layout change** the
moment the same vault is opened there. Two concrete failure modes follow
directly from this:

1. **Window-geometry amnesia**: any Obsidian session on the vault destroys
   `_corbomite`, so Corbomite's window size/position/sidebar widths/plugin
   state silently reset next time Corbomite opens that vault.
2. **Sync-conflict bait**: for a vault synced via Syncthing or git (common
   for this user's own vaults — see root `~/dev/CLAUDE.md`), Corbomite is
   now writing machine-local UI chrome state into a file that two
   independent apps both churn on every layout tweak, multiplying
   conflict/merge surface for no interop benefit — none of `_corbomite`'s
   contents are meaningful to Obsidian or to Corbomite running on a
   *different* machine.

### Decision

**Only Obsidian-schema keys are permitted in `.obsidian/workspace.json`.**
That means: `main`, `active`, and everything currently captured into
`m_unknownRoot` (`left`, `right`, `floating`, `lastOpenFiles`, and any
future key Obsidian adds — passthrough per §1). `_corbomite` and
`left-ribbon` are removed from `SessionManager::doSave()`'s output
entirely — **not migrated**. Corbomite has no installed users yet (no
dogfooding, no external testers); there is nothing to preserve, so Phase
L2 does not need a migration shim. It only needs a one-line denylist fix
(below) so old dev-build `_corbomite`/`left-ribbon` keys, if present in a
developer's own test vault, are dropped on next save rather than being
swept into `m_unknownRoot`'s passthrough and living forever as zombie
keys — `m_unknownRoot` exists to preserve *future Obsidian* keys, not
Corbomite's own retired ones.

Corbomite-native state was previously going to be a single machine-local
blob (option (a) from the original plan draft). On review that conflates
two genuinely different kinds of state with different sync semantics.
**Split into three tiers**, not two:

| Tier | Contents | Lives at | Sync behavior |
|---|---|---|---|
| **1 — Obsidian-schema** | `main`, `active`, `left`/`right`/`floating`/`lastOpenFiles`, any future Obsidian key | `.obsidian/workspace.json` | Whatever the user already syncs the vault with (Syncthing/git); Obsidian owns the churn |
| **2 — Vault-portable, Corbomite-native** | expanded folders, left-ribbon layout, sidebar collapsed/expanded (a boolean, not a pixel width) — content-shaped preferences a user plausibly wants to follow the vault between machines | `.obsidian/corbomite/state.json` — new file, inside the vault but in its own subfolder Obsidian never reads or writes | Rides the *same* channel the vault already syncs with — no new sync mechanism invented. Obsidian never opens this file, so there is no wholesale-rewrite race with it (unlike today's `_corbomite` tail inside `workspace.json`) |
| **3 — Machine-local** | window geometry/position, sidebar pixel widths, `pluginSessionState` (ephemeral per-toolview UI state, e.g. tree-expand — see below, distinct from real plugin data) | `~/.local/share/corbomite[-dev]/vaults/<vaultId>/session.json` | Never synced, by design |

**Why split tier 2 out instead of folding it into tier 3 (app-data,
fully machine-local):**

| | Fold into tier 3 (machine-local only) | Tier 2 (vault-portable, own file) |
|---|---|---|
| Expanded folders, ribbon layout | Wrong scope — these are *content preferences about this vault*, the same category Obsidian itself round-trips (`lastOpenFiles`, `left`/`right`) via `workspace.json`; a user reopening the same vault on the laptop reasonably expects the tree to look the same | Correct scope — matches user expectation, matches Obsidian's own precedent for what "workspace state" means |
| Window geometry, pixel widths | Correct scope — screen-relative, not vault-relative; syncing a 1440p rect onto a 13" laptop is actively wrong | N/A — stays in tier 3 |
| Sync/conflict surface vs. today's `_corbomite`-in-workspace.json bug | N/A | Zero *new* risk versus what the vault's own sync already tolerates for `workspace.json` itself — tier 2 just adds one more file to the same sync unit, and Obsidian doesn't contend for it the way it contends for `workspace.json` |
| Precedent in this codebase | `SQLiteIndex`/`MetadataCache` write derived, machine-local vault artifacts outside the vault folder under `AppDataLocation` (`libs/storage/src/SQLiteIndex.cpp`) — matches tier 3 | `PluginDataStore` (`libs/vault/src/PluginDataStore.cpp`) already writes `data.json` *inside* each plugin's folder under `.obsidian/plugins/<id>/`, vault-portable by construction, matching real Obsidian plugin behavior — matches tier 2 |

**A finding that motivated the split:** what's currently called "plugin
session state" in `_corbomite` (`SessionManager::setPluginSessionState`,
wired at `MainWindow.cpp:978-999`) is *not* the same thing as plugin data.
`PluginDataStore` already gives plugins a correct, vault-portable home for
real settings (`.obsidian/plugins/<id>/data.json`, exactly how upstream
Obsidian plugins persist config). What lives in `_corbomite.pluginSessionState`
is a narrower thing — ephemeral per-toolview UI restore state (tree-expand
on a plugin's panel) — and belongs in tier 3, not conflated with tier 2 or
with `PluginDataStore`'s tier.

**`vaultId`**: no stable per-vault id exists in the codebase yet (grep
confirms). Rather than hash the vault's filesystem path (breaks on
rename/move, and doesn't help correlate machines anyway since tier 3 is
machine-local by design), Phase L2 generates a UUID once on first open and
stores it in tier 2 (`.obsidian/corbomite/state.json`) — it travels with
the vault, stays stable across a rename/move on one machine, and gives
tier 3's directory name something durable to key off of.

**Denylist fix (Phase L2, one line):** `SessionManager`'s Obsidian-key
passthrough must explicitly drop `_corbomite` and `left-ribbon` on load
instead of forwarding them into `m_unknownRoot`. Without this, any
existing dev-build vault would carry the retired keys forward forever the
first time this doctrine's own code runs, defeating the point of not
needing a migration shim.

---

## 3. What this unblocks

Phase L2 ("One writer, full fidelity") implements the migration described
here:

1. Split `SessionManager` into three writers, one per tier — the
   Obsidian-schema file (still consuming the *full* `serialize()` payload,
   closing finding B1's `floating`/`lastOpenFiles` drop at
   `MainWindow.cpp:851-861`), the new vault-portable
   `.obsidian/corbomite/state.json` (expanded folders, ribbon layout,
   sidebar visibility), and the new machine-local
   `<vaultId>/session.json` (window geometry, pixel widths,
   `pluginSessionState`).
2. Denylist fix: strip `_corbomite`/`left-ribbon` on load instead of
   passthrough (no migration — see §2, "no installed users yet").
3. B3 (empty split/tabs `id`, missing `dimension`) and B4 (unbounded
   process-global sidecar statics in `WorkspaceSerializer.cpp:34-61`) are
   schema-at-rest bugs under this same doctrine — they belong in Phase L2's
   golden-fixture test alongside the `_corbomite` migration, not treated as
   a separate concern.
4. Phase L2's golden test (a real Obsidian-authored `workspace.json`
   fixture, asserting load→save preserves every Obsidian key byte-for-byte
   modulo the known-allowed rewrites this doc licenses) is the executable
   form of §1's doctrine statement.

Phase L1 (teardown unification) does not depend on this doc and can proceed
in parallel — it is entirely on the in-memory side of the boundary this
doctrine draws.

---

## 4. Non-decisions (explicitly deferred)

- Exact on-disk shape of `.obsidian/corbomite/state.json` and
  `session.json` (key names, versioning) — Phase L2 implementation detail;
  this doc fixes the tier boundary and file locations, not the schema.
- `eState.scroll` (fraction vs. Obsidian's line number, finding B6) —
  unaffected by this doc; it's a value-encoding bug inside an already-legal
  key, tracked on the punch list.
- Sidedock (`left`/`right`) full modeling — stays a Cluster F non-goal per
  the cluster plan §4; this doc only confirms the passthrough-until-dirty
  behavior in `SessionManager::doSave()` (lines 258–266) is consistent with
  §1 (it treats `left`/`right` as opaque Obsidian-schema data, never
  Corbomite-owned).
