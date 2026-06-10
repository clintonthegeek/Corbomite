# Replace the Ribbon Slot with a Second KToolBar

**Date:** 2026-04-20
**Status:** Shipped 2026-04-20/25
**Scope:** Delete `Corbomite::RibbonSlot`; replace with a KDE-native KToolBar pair; persist per-vault state into `workspace.json['left-ribbon']` via the existing `SessionManager`.

## Motivation

Obsidian's "ribbon" (`WorkspaceRibbon`, `workspace.md §7`) is, functionally, a vertical icon-only toolbar. The only Obsidian-specific behaviors layered on top are:

1. Per-vault persistence of icon order + hidden items (`workspace.json['left-ribbon'].hiddenItems` — map key order = runtime order).
2. In-place drag-reorder of individual icons.
3. Right-click → hide *this* icon.
4. A dedicated plugin contribution surface (`Plugin.addRibbonIcon`).

Obsidian carries all of its command chrome on the ribbon because it has no menubar and no traditional toolbar. Corbomite has both, so the current `RibbonSlot` left-dock is a redundant third chrome surface whose entries already exist on the main toolbar.

KDE users expect `KToolBar` + *Settings → Configure Toolbars* for command chrome. Left-docking clashes visually with the KateMDI side panels. We translate the ribbon concept rather than clone it.

## Decision record

Captured from brainstorming 2026-04-20:

- **Two toolbars, not one.** A fixed programmable main toolbar the user owns across vaults, and a dynamic per-vault/per-context second toolbar. The second toolbar may be empty; that is normal.
- **Default position of the second toolbar: top, right of the main toolbar.** Not left-docked. Users may drag it to `Qt::LeftToolBarArea` themselves via standard KToolBar drag behavior.
- **Drop drag-reorder and per-item right-click hide.** Users reorder and hide via standard *Configure Toolbars*. Documented in backlog as a possible future experiment.
- **Controller is vault-owned.** `RibbonStateController` lifetime follows `Corbomite::Vault`, consistent with the post-Cluster-Q.0 pattern where `libs/vault` owns per-vault state.
- **Title-keyed identity, matching Obsidian's quirk exactly.** Two `addRibbonIcon` calls with the same title collide silently; the second is dropped. `PLUGIN-API-SKETCH.md §11.1` documents this as a preserved compat invariant. We keep it.

## Architecture

### The two toolbars

| Toolbar | objectName | Managed by | Default position | Persists to |
|---|---|---|---|---|
| Main toolbar | `mainToolBar` (existing) | KXMLGUI via `corbomiteui.rc` | Top | `~/.config/corbomite-devrc` (global) |
| Ribbon toolbar | `ribbonToolBar` (new) | Programmatic, outside KXMLGUI | Top, immediately right of `mainToolBar` | `<vault>/.obsidian/workspace.json['left-ribbon']` (per-vault) |

The ribbon toolbar is **not listed** in `corbomiteui.rc`. KXMLGUI's *Configure Toolbars* dialog only edits toolbars whose actions come from an `.rc` file, so the ribbon toolbar is excluded by construction. Its own context menu (right-click on the toolbar handle) still offers *Lock Toolbars*, *Text Position*, *Icon Size*, etc. — standard KToolBar affordances.

### Component boundaries

**`Corbomite::RibbonToolBar`** (thin `KToolBar` subclass)

- Lives in `src/app/` alongside `MainWindow` — UI glue, not domain logic, not a standalone library.
- API (small divergence from today's `RibbonSlot`; consumers in `MainWindow.cpp` — three call sites — get trivial churn):
  ```cpp
  using Handle = QString;  // == the full ribbon-item id, e.g. "core:quick_switcher"
                           //    or "<pluginId>:<title>" for plugins
  Handle addRibbonIcon(const Handle &id,
                       const QIcon &icon,
                       const QString &title,
                       std::function<void()> onActivated);
  bool   removeRibbonIcon(const Handle &id);
  int    iconCount() const;
  bool   hasIcon(const Handle &id) const;

  // For the controller:
  QStringList iconIdsInOrder() const;
  void setIconVisible(const Handle &id, bool visible);
  bool isIconVisible(const Handle &id) const;

  signals:
    void iconAdded(const QString &id);
    void iconRemoved(const QString &id);
  ```
- Owns a `QHash<QString, QAction*>` keyed on id. Collision on id → return empty `Handle`.
- **Title-collision invariant (Obsidian compat).** When `id` is formed as `"<pluginId>:<title>"` by the plugin-API layer, two plugin calls with the same title produce the same id and collide silently — this is Obsidian's documented quirk (`PLUGIN-API-SKETCH.md §11.1`). For core callers, the id is a stable internal string chosen once; no collision risk.
- Does **not** touch JSON or `SessionManager`. Emission-only; the controller observes.

**`Corbomite::RibbonStateController`**

- Lives in `libs/vault/src/` (vault-lifecycle-scoped).
- Constructed by `Corbomite::Vault` alongside other per-vault services.
- Dependencies (constructor-injected): a `RibbonToolBar*` and a `SessionManager*`.
- Responsibilities:
  1. On vault open (i.e. after `SessionManager::load()` has populated state), read the `['left-ribbon']` sub-object and apply `hiddenItems` to whichever icons are currently registered. Icon-order is driven by the key order of the `hiddenItems` JSON object (Obsidian convention: keys enumerate the full known-item set, value `true` = hidden).
  2. On `RibbonToolBar::iconAdded`/`iconRemoved` and on visibility changes: write back via a new `SessionManager::setLeftRibbonState(const QJsonObject&)` setter. `SessionManager` already debounces and schedules saves; the controller does not own a timer.
  3. On vault close: detach from signals; no explicit flush needed (SessionManager's existing shutdown path handles it).
- Icons registered *before* vault open (e.g. by core code at MainWindow construction) are applied retroactively when the vault's `hiddenItems` lands.

**`Corbomite::SessionManager` additions**

- `QJsonObject leftRibbonState() const;`
- `void setLeftRibbonState(const QJsonObject &state);` → triggers `scheduleSave()`.
- Reads/writes the `['left-ribbon']` key directly (it currently passes through untouched via `m_unknownRoot` — we promote it to a typed field).
- Unknown sub-keys inside `['left-ribbon']` (future Obsidian additions) are preserved via the same unknown-key pattern.

### Data flow

```
MainWindow ctor ─┬─▶ addToolBar(mainToolBar)         // existing, KXMLGUI
                 └─▶ addToolBar(ribbonToolBar)       // new, top-right-of-main

Vault open ─▶ RibbonStateController::onVaultOpened()
                  │
                  ├─ reads SessionManager::leftRibbonState()
                  └─ applies visibility + iteration order to RibbonToolBar

Plugin/core code ─addRibbonIcon─▶ RibbonToolBar
                                      │ iconAdded
                                      ▼
                          RibbonStateController updates JSON →
                          SessionManager::setLeftRibbonState() →
                          SessionManager debounced save (1 s)
```

### `workspace.json['left-ribbon']` schema

Matches Obsidian exactly (`workspace.md §2`, lines 165):

```json
{
  "left-ribbon": {
    "hiddenItems": {
      "switcher:Open quick switcher": false,
      "graph:Open graph view": true,
      "daily-notes:Open today's daily note": false
    }
  }
}
```

- Key = ribbon item id. For core-registered icons, use a stable internal id (e.g. `"core:quick_switcher"`). For plugin-registered icons, format is `"<pluginId>:<title>"` per `PLUGIN-API-SKETCH.md §5.11`.
- Value `true` = hidden; `false` or absent = visible.
- Map key order encodes runtime order. Since Corbomite drops drag-reorder, we preserve whatever order Obsidian wrote on its last save, and append newly-registered ids at the end.

## Deletion and migration

### Removed

- `src/app/RibbonSlot.h`, `src/app/RibbonSlot.cpp`
- `tests/dialogs/tst_ribbonslot.cpp` (replaced)
- `MainWindow::setupRibbon()` body (replaced with `setupRibbonToolBar()`)
- The three hardcoded entries (New note / Quick switcher / Graph view). Their underlying actions (`file_new_note`, `quick_switcher`, `graph_view`) already exist in `actionCollection()` and remain available on the main toolbar and in the command palette / shortcuts.

### Added

- `src/app/RibbonToolBar.{h,cpp}`
- `libs/vault/include/corbomite/vault/RibbonStateController.h`
- `libs/vault/src/RibbonStateController.cpp`
- `tests/dialogs/tst_ribbontoolbar.cpp`
- `libs/vault/tests/tst_ribbonstatecontroller.cpp`
- `SessionManager::leftRibbonState()` / `setLeftRibbonState()`.

### Backwards compat

- Existing `workspace.json` files that already contain a `['left-ribbon']` sub-object (preserved as unknown-root) continue to round-trip. On next save after this change lands, they are re-serialized identically.
- No user-facing data migration needed. Fresh Corbomite-only vaults produce a `['left-ribbon']` entry only after the first icon visibility change or first plugin-ribbon registration — otherwise the key is simply absent, matching Obsidian's behavior for ribbons at default state.

## Testing

- **`tst_ribbontoolbar`**: add/remove, title-collision silent-drop (returns empty Handle, second callback not bound), `setIconVisible` round-trips via `isIconVisible`, signals fire on add/remove.
- **`tst_ribbonstatecontroller`**: given a fixture `workspace.json` with `hiddenItems`, apply to a pre-populated `RibbonToolBar` and confirm visibility + iteration order. Toggle an icon, confirm `SessionManager::leftRibbonState()` reflects the change. Register a new icon, confirm it is appended to the key order.
- **Integration (existing MainWindow test harness)**: open a vault, confirm `ribbonToolBar` is present at top-right of `mainToolBar`, confirm the controller is wired.

## Out of scope (documented, deferred)

- Drag-reorder and per-item right-click hide. Added to `docs/backlog.md` under UI Ergonomics as "Ribbon-style toolbar micro-UX experiments" — rationale: nice-to-have, not worth the subclass complexity until a user complaint lands.
- `RibbonProxy` in `PluginContext` — existing `docs/backlog.md:101` item stays open; no plugin consumer yet. When the first plugin needs ribbon access, it will register via the same `addRibbonIcon` entrypoint, routed through a permission-gated proxy.
- Right-edge ribbon (`WorkspaceRibbon("right")`). Obsidian supports it; almost nobody uses it. Skip until demand appears.
- `Platform.canDisplayRibbon` capability flag. Desktop always can; mobile is not a Corbomite target.

## Open questions

None remaining after brainstorming. Controller ownership: Vault-owned (confirmed). Identity keying: id-based (`"<pluginId>:<title>"` for plugins, stable internal id for core), matching Obsidian's `hiddenItems` key format (confirmed). Default toolbar position: top-right (confirmed).
