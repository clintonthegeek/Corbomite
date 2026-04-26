# Plugin enable-state cross-app compromise (Corbomite ⇄ Obsidian)

**Status:** accepted (2026-04-26)
**Drives:** punch-list P0 — *"Wire `core-plugins.json` and `community-plugins.json` to `PluginManager` so toggle state transfers Corbomite ⇄ Obsidian"*
**Audit reference:** [`docs/audit-2026-04-26/settings.md`](../../audit-2026-04-26/settings.md) §"On-disk schema compatibility matrix (per-file)" + §"End-user settings UI parity"

## Problem

Corbomite already reads + writes `.obsidian/core-plugins.json` and
`.obsidian/community-plugins.json` faithfully at the I/O layer
(`VaultConfig::readCorePlugins/writeCorePlugins/readCommunityPlugins/
writeCommunityPlugins`). It also persists plugin enable-state in
KConfig (`PluginManager::loadEnabledStateFromConfig` /
`writeEnabledState`). **The two halves are not connected.** Toggling a
plugin in Corbomite does not update either JSON file; opening a vault
that Obsidian has touched does not pick up Obsidian's toggles.

A naive bidirectional sync runs into three real obstacles:

1. **ID space mismatch.** Corbomite's internal plugins use slugs like
   `corbomite_backlinks`, `corbomite_outline`, `corbomite_filerecovery`.
   Obsidian's core plugins use bare slugs (`backlinks`, `outline`,
   `file-recovery`). They are **not 1:1**: some Obsidian core plugins
   have no Corbomite equivalent (e.g. `audio-recorder`,
   `slides`); some Corbomite plugins have no Obsidian counterpart
   (`note-stats` reference plugin); names diverge even where the
   feature matches (`backlink` singular in Obsidian, `backlinks` plural
   in Corbomite).
2. **Schema asymmetry.** `core-plugins.json` is `{id: bool}` (explicit
   on/off). `community-plugins.json` is a top-level array of strings
   (presence = enabled; absence = "not installed AND not enabled" —
   Obsidian conflates installation with enablement for community
   plugins). Corbomite has no separate "installation" concept; every
   plugin discovered on disk is "installed".
3. **Trust model divergence.** Corbomite's `X-Corbomite-Trusted` flag
   plus the Permission-grant dialog has no Obsidian analogue. A plugin
   trusted by Corbomite is meaningless to Obsidian; a community plugin
   in Obsidian (which Obsidian's safe-mode flow has gated) needs
   Corbomite's permission dialog the first time it loads.

## Decision (the compromise)

**We accept lossy bidirectional sync** for plugins that have a declared
Obsidian counterpart, **leave KConfig as the durable source of truth**
inside Corbomite, and **mirror to the JSON files** so an Obsidian
session sees a coherent view. Obsidian's edits are picked up on
**vault open** (a "first-open reconciliation"); subsequent
intra-session toggles are KConfig-authoritative until next vault open.

The four concrete sub-decisions:

### 1. ID mapping: manifest field + alias dict fallback

Corbomite plugins declare their Obsidian counterpart via a new
manifest field:

```json
"KPlugin": { "Id": "corbomite_backlinks", ... },
"X-Obsidian-Id": "backlink"
```

A plugin with **no** `X-Obsidian-Id` is treated as Corbomite-only and
**does not** participate in the JSON sync — its KConfig state is
private to Corbomite.

For the 8 internal plugins shipped with Cluster Q, the mapping is
hard-coded into `PluginManager` as the initial alias dict (until each
internal plugin's `metadata.json` is updated to carry the new field):

| Corbomite ID | Obsidian ID | Category |
|---|---|---|
| `corbomite_backlinks` | `backlink` | core |
| `corbomite_outline` | `outline` | core |
| `corbomite_tag-pane` | `tag-pane` | core |
| `corbomite_word-count` | `word-count` | core |
| `corbomite_random-note` | `random-note` | core |
| `corbomite_filerecovery` | `file-recovery` | core |
| `corbomite_starred` | `bookmarks` | core |
| `corbomite_note-stats` | *(none — Corbomite-only)* | — |

### 2. Direction of authority: JSON wins on vault-open, KConfig wins thereafter

- **On `Vault::open`:** read `core-plugins.json` and
  `community-plugins.json`, translate Obsidian IDs to Corbomite IDs
  via the alias dict, **overlay** that state onto KConfig before
  `loadEnabledStateFromConfig` runs. Plugins **without** an
  `X-Obsidian-Id` are unaffected.
- **During the session:** `enablePlugin` / `disablePlugin` write to
  KConfig as before, **and additionally** write the corresponding JSON
  file if the plugin has an `X-Obsidian-Id`. KConfig is the
  intra-session source of truth.
- **On the next `Vault::open`:** any out-of-band edit in
  `core-plugins.json` (e.g. by Obsidian) is re-overlaid. Diff is
  resolved JSON-wins.

This is asymmetric on purpose: Obsidian users editing toggles between
Corbomite sessions get their changes respected; Corbomite users
toggling rapidly during a session don't pay an O(n) JSON-write tax
beyond the file the toggle directly affects.

### 3. Core vs community partition: `metaData.trusted()` is the discriminator

- `info.metaData.trusted() == true` ⇒ write to `core-plugins.json`
  (object form, `{obsidianId: bool}`).
- `info.metaData.trusted() == false` ⇒ write to
  `community-plugins.json` (array form, presence = enabled).

Rationale: Cluster Q established `X-Corbomite-Trusted` as the
"shipped-with-the-app and granted permissions automatically" flag.
That semantically aligns with Obsidian's "core plugin" status. We
**ship without a separate `X-Obsidian-Category` manifest field** for
now — if a plugin author finds the trusted-implies-core mapping wrong
for their case, we'll add the explicit override field. (Tracked as a
P5 follow-up if it bites; not pre-emptive.)

### 4. Dual-write on toggle, only for plugins with an Obsidian ID

`writeEnabledState` becomes:

```cpp
void PluginManager::writeEnabledState(const QString &id, bool enabled) {
    // Always: KConfig (intra-session authoritative).
    KConfigGroup grp(m_config, "Plugins");
    grp.writeEntry(id + "Enabled", enabled);
    grp.sync();

    // Conditionally: JSON mirror, only for plugins with X-Obsidian-Id.
    const QString obsId = obsidianIdFor(id);
    if (obsId.isEmpty() || !m_vaultConfig) return;

    if (info->metaData.trusted()) {
        // core-plugins.json: {obsId: enabled}
        auto cur = m_vaultConfig->readCorePlugins().value_or(VaultConfig::CorePlugins{});
        cur.raw.insert(obsId, enabled);
        m_vaultConfig->writeCorePlugins(cur);
    } else {
        // community-plugins.json: array, presence = enabled
        auto cur = m_vaultConfig->readCommunityPlugins().value_or(QStringList{});
        cur.removeAll(obsId);
        if (enabled) cur.append(obsId);
        m_vaultConfig->writeCommunityPlugins(cur);
    }
}
```

`PluginManager` therefore needs a `VaultConfig *` reference (set by
`CorbomiteApp` after vault open, cleared on vault close). Plugins
discovered before a vault is open behave as today (KConfig only).

## What we explicitly do NOT promise

- **No reverse alias** for plugins Corbomite doesn't ship. Obsidian
  enabling `daily-notes` (we have no equivalent) is silently ignored
  on Corbomite-side. The JSON entry survives untouched (we only
  insert/remove keys for plugins we know about).
- **No mid-session JSON-watch.** Editing `core-plugins.json` while
  Corbomite is running does not toggle plugins. (Would need a
  `QFileSystemWatcher` plumbed through `Vault::watch`; deferred to the
  P4 `onExternalSettingsChange()` punch-list item.)
- **No installation/uninstallation semantics.** Corbomite never
  removes a plugin from `community-plugins.json` "because it's
  uninstalled" — Corbomite has no uninstallation flow. If Obsidian
  removes the entry, the next vault-open in Corbomite respects that
  (treats it as "disabled").
- **No conflict detection.** If Obsidian and Corbomite both edit while
  not running and the disk states are inconsistent (e.g. KConfig
  says enabled, JSON says disabled), JSON wins on next open.
  No prompt, no diff. This is intentional — Obsidian-edits-while-
  Corbomite-closed is by far the more common case to support.
- **No promotion of Corbomite-only plugins into the Obsidian view.**
  `note-stats` won't ever appear in `community-plugins.json`. If an
  Obsidian user wants to see the Corbomite-only plugin in their UI,
  they can't — that's the cost of having no shared identity.

## Test surfaces (must exist before this is "shipped")

1. Round-trip: open vault with `core-plugins.json` saying
   `{"backlink": false}`, verify Corbomite's `corbomite_backlinks`
   loads disabled.
2. Round-trip: with vault open, toggle `corbomite_backlinks` off,
   verify `core-plugins.json` now contains `{"backlink": false}`.
3. Round-trip: with vault open, toggle a non-trusted plugin
   (synthetic test fixture, since all internal plugins are trusted)
   on, verify it appears in `community-plugins.json`.
4. Preservation: a `core-plugins.json` containing keys we don't know
   about (`{"daily-notes": true, "command-palette": true}`) must
   survive a Corbomite toggle of an unrelated plugin without losing
   those keys.
5. Plugin-without-`X-Obsidian-Id` (`note-stats`): toggling it must
   leave both JSON files untouched.

These tests live in `libs/plugin/tests/` (PluginManager-side) and
`tests/storage/tst_obsidian_vault_roundtrip.cpp` (VaultConfig-side
already covered).

## Why we accept the lossiness

The alternative — full bidirectional sync with conflict detection,
mid-session watching, ID negotiation per plugin — buys near-zero
real-world value: vault users either run Corbomite or Obsidian at a
given moment, and the JSON-wins-on-open rule covers the cross-tool
session boundary. The compromise's lossy edges (Corbomite-only plugins
invisible to Obsidian; mid-session JSON edits ignored; Obsidian-only
plugins silently no-op) are tolerable because none of them risk
**vault-format corruption** — the P0 framing this punch-list item
sits under. We are buying *toggle-state transfer*, not full
plugin-identity reconciliation.
