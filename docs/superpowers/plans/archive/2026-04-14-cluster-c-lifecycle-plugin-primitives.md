# Cluster C — Lifecycle / plugin primitives

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster C.

**Covers:** P0.7 (vault-switch process-death pattern), P2.32 (command registry with checkCallback/editorCallback/editorCheckCallback variants), P2.33 (plugin-id-prefixed hotkeys), P3.1 (`Component` lifecycle), P3.2 (`Events` mixin facade over Qt signals), P3.3 (`Scope` hierarchical hotkey stack), P3.5 (command registry — foundation for P2.32).

## Goal

Build the **two foundation primitives** (`Component` lifecycle + `Events` mixin) and the **hierarchical key-handler stack** (`Scope`), plus fix the **vault-switching crash** at its architectural root (it's not a bug — Obsidian destroys the window on vault switch). These are the load-bearing substrates the entire plugin-ready surface depends on; `PLUGIN-API-SKETCH.md §3–§4` treats them as preconditions.

Cluster grouped because all four work together at the lifecycle boundary: a `Plugin` is a `Component`, subscribes via `Events.registerEvent`, registers hotkeys via `Scope`, and unloads when the vault-owning MainWindow is destroyed. Build separately and they mis-align on cleanup ordering.

## Audit references

- **Vault-switch process-death:** `domains/core.md §1` — `openVaultChooser()` → `ipcRenderer.sendSync("starter") + window.close()`. **No in-process vault swap exists in Obsidian.** `MEMORY.md` already flags Corbomite's in-process swap as the crash cause; fix is Kate-session-pattern destroy-and-recreate MainWindow.
- **`Component` lifecycle spec:** `domains/ui-bundle.md §1` (sub-subheading `components/Component.js`) — `load`, `unload`, `addChild`, `removeChild`, `onload`, `onunload`, `registerDomEvent`, `registerEvent`, `registerInterval`. LIFO cleanup on unload.
- **`Events` mixin:** `domains/core.md §1` — `on`, `off`, `offref`, `trigger`, `tryTrigger`. `tryTrigger` re-throws async via `setTimeout(…, 0)`. Internal storage `_ = { eventname: [listeners] }`. `on` returns an `EventRef`; `offref(ref)` is the unsubscribe.
- **`Scope`:** `domains/core.md §1` (and `domains/platform.md` — Keymap is a duplicate-extracted `Scope`) — `register(modifiers, key, fn)`, `unregister`, `handleKey`. Parent-chain lookup (child-first). Used by `Modal`, `Menu`, `keymap.global`. Qt has no equivalent; `KActionCollection` + `QShortcut` don't stack.
- **Commands with variant callbacks:** `domains/plugin.md §10` + `domains/core.md §7` — `callback`, `checkCallback(checking) → bool`, `editorCallback(editor, view)`, `editorCheckCallback(checking, editor, view) → bool`. The "check" variants let the palette grey out when the command isn't applicable (no editor, no file).
- **Plugin-id-prefixed hotkeys:** `domains/plugin.md §4` + `domains/settings.md §3` (`hotkeys.json` schema `{commandId: Hotkey[]}` where commandId is `<pluginId>:<localId>`). Also `plugin.md` flagged the `addCommand` in-place mutation of `cmd.id` to the prefixed form.
- **`addRibbonIcon` title-collision bug, `addCommand` double-namespacing on re-call, status-bar sanitiser non-global regex** — preserved in compat. `domains/plugin.md §3–§4`.
- **Kate's session pattern** — already in `MEMORY.md` as `reference_kate_sessions.md`.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::Component` | `libs/core/src/Component.{h,cpp}` | Universal lifecycle base; virtual `onload`/`onunload`; owns `QVector<QPointer<Component>> m_children`; LIFO unload |
| `Corbomite::EventRef` | `libs/core/src/Events.h` | Opaque handle returned by Events subscriptions |
| `Corbomite::Events` | `libs/core/src/Events.{h,cpp}` | Facade; preferred: template mixin. `on`/`off`/`offref`/`trigger`/`tryTrigger`. Backs with QMetaObject for async-rethrow parity |
| `Corbomite::Scope` | `libs/core/src/Scope.{h,cpp}` | Parent pointer + key-table. `registerBinding(mods, key, fn)`. `handleKey` walks up chain |
| `Corbomite::ScopeManager` | `src/app/ScopeManager.{h,cpp}` | Installs `QApplication::installEventFilter` that walks the active-scope stack on every `QKeyEvent` |
| `Corbomite::Command` + `CommandRegistry` | `libs/core/src/Command.{h,cpp}` | `id`, `name`, `icon`, variant callbacks (function-type discriminated via `std::variant`) |
| `Corbomite::Hotkey` | `libs/core/src/Hotkey.{h,cpp}` | Modifier-set + key; serialises to/from `hotkeys.json` format (cite VAULT-FORMAT §3.hotkeys.json) |
| `Corbomite::PluginInstance` (stub) | `libs/core/src/PluginInstance.{h,cpp}` | Subclass of `Component`; scaffolded here so Command + Scope have a plugin-id context |
| `Corbomite::SessionDestroyer` | `src/app/SessionDestroyer.{h,cpp}` | Kate-pattern: on vault switch, persist state, `MainWindow::close()`, launcher spawns new MainWindow with target vault |

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| Component lifecycle | `~/src/kde/src/kxmlgui/` (`KXMLGUIClient`) + `~/src/kde/src/kparts/` (`ReadWritePart`) | Hierarchical widget lifecycle with resource cleanup |
| Events mixin on QObject | Qt6 native — `QObject::connect` with `Qt::QueuedConnection` for async-rethrow pattern | Queued-connection emulates `setTimeout(0)` rethrow |
| Scope hierarchical keymap | `~/src/kde/src/kate/` (search for `commandBar`), `~/src/kde/src/kdevelop/` (KeyBindings), `~/src/kde/src/kwidgetsaddons/` | Concrete prior art for context-stacked hotkeys |
| **Session destroy/rebuild** | **`~/src/kde/src/kate/apps/lib/session/`** — canonical pattern for our vault-switch fix | The single most important reference in this cluster |
| Variant-callback command | `~/src/kde/src/ktexteditor/` (`KTextEditor::Command`) — single-action commands, not variant | Partial; we combine `KActionCollection` for keying with our own variant-payload |
| Hotkey serialisation | `~/src/kde/src/kxmlgui/src/kactioncollection.cpp` (`writeSettings`) + Qt6 native `QKeySequence` | Readable base; may need pluginId-prefix convention layered on |
| Command-palette UX | `~/src/kde/src/kwidgetsaddons/` (`KCommandBar` source — already in Corbomite via KF6) | Already in use; reuse as palette frontend |

## Work breakdown

**Phase 1 — Component + Events (libs/core):**
1. Implement `Corbomite::Component`. Virtual `onload`, `onunload`. `addChild(Component*)` takes ownership; `unload()` calls `onunload` on children LIFO, then self. `registerInterval(ms, callback)` returns timer id that's auto-cleared on unload. `registerDomEvent` (not relevant — Qt uses event filters; instead provide `registerQObjectConnection(QMetaObject::Connection)` that disconnects on unload).
2. Implement `Corbomite::Events` as a CRTP mixin: `class Vault : public QObject, public Events<Vault>`. Methods: `EventRef on(name, fn)`, `off(name, fn)`, `offref(EventRef)`, `trigger(name, args...)`, `tryTrigger(name, args...)` where the latter emits via `QMetaObject::invokeMethod(this, …, Qt::QueuedConnection)` for async-rethrow parity.
3. Unit test: spawn a `Component` tree, register interval + event + child, call unload, assert all resources released.
4. Unit test: `Events::tryTrigger` with a throwing listener — subsequent listeners still fire, exception surfaces in the next event loop iteration.

**Phase 2 — Scope + ScopeManager:**
5. Implement `Corbomite::Scope`. `parent: QPointer<Scope>`, `bindings: QHash<QPair<Qt::KeyboardModifiers, int>, Callback>`. `handleKey(QKeyEvent*) → bool` walks child→parent, first hit wins.
6. Implement `ScopeManager` as a singleton installed on `QApplication` via `installEventFilter`. Stack-of-scopes (LIFO push/pop). `Modal`/`Menu` push their own scope on open, pop on close.
7. Integration: replace any existing `QShortcut` usage with `Scope::registerBinding` in the main editor/modal paths. `KActionCollection` remains for menu-bar actions (they don't need scope-stacking).
8. Test: open a modal with `Esc` bound, focus in editor also has `Esc` bound — modal's `Esc` wins while modal is open, editor's `Esc` wins when modal is closed.

**Phase 3 — Command registry + Hotkey:**
9. `Corbomite::Command { id, name, icon, callback }` where `callback` is a `std::variant<SimpleCallback, CheckCallback, EditorCallback, EditorCheckCallback>`. Registry: `addCommand`, `removeCommand`, `findCommand(id)`, `listCommands() → QVector<Command*>`, `executeById(id)` (dispatches correct variant).
10. `Hotkey` serialiser/parser for `hotkeys.json`. Preserve unknown-key tail (any future Obsidian extension survives).
11. Bind the command registry to `KCommandBar` (Corbomite already uses it) — palette rows come from `listCommands()` filtered via `checkCallback`.
12. When Corbomite's plugin system lands, `PluginInstance::addCommand(cmd)` mutates `cmd.id = pluginId + ":" + cmd.id` (compat with Obsidian's `addCommand` in-place mutation) before registry insert.

**Phase 4 — Vault-switch destroy/rebuild:**
13. Persist session to `WorkspaceState` (Cluster B) on pre-close.
14. Add `SessionDestroyer::switchVault(newVaultPath)` — persists state, calls `MainWindow::close()`. Main `Application` intercepts the `aboutToQuit` path: if a `newVaultPath` is set, spawn a new MainWindow with that vault; else quit.
15. Replace current in-process vault swap code (`VaultService::openVault` mid-session) with a trap: it calls `SessionDestroyer::switchVault` instead.
16. Manual test: open vault A, open vault B — window destroys + recreates, session restored to B's workspace.json.

## Explore-agent dispatch prompts

**Prompt 1 — Kate session pattern extraction:**
> Read Kate's session management code at the local source tree `~/src/kde/src/kate/apps/lib/session/`. Do NOT clone from upstream — local source is current. Identify: (a) how Kate persists session state before a session switch, (b) how Kate spawns a new MainWindow for a different session, (c) the launcher/chooser integration path, (d) what Qt signals/KDE APIs are load-bearing. Report a translation plan for Corbomite's `SessionDestroyer` class. Under 700 words.

**Prompt 2 — Events CRTP vs multiple-inheritance:**
> Evaluate whether `Corbomite::Events<T>` should be a CRTP mixin vs a plain non-template class inherited alongside `QObject`. Considerations: Qt MOC restrictions on templates, compile-time cost of CRTP, ability for subclasses to have their own Q_SIGNALS, virtual-dispatch overhead. Cite exact constraints from Qt6 docs. Recommend a design. Under 500 words.

**Prompt 3 — QApplication event filter for Scope:**
> Evaluate `QApplication::installEventFilter` as the dispatch mechanism for `Corbomite::Scope`. Considerations: (a) ordering vs widget-local key events, (b) interaction with `QShortcut` resolution, (c) edge case of focus-change mid-handler, (d) whether focus-widget-lookup should gate scope application. Compare with the alternative of per-widget `QObject::installEventFilter` on the active leaf. Under 500 words. Recommendation.

## Definition of done

- `Corbomite::Component`, `Events<T>`, `Scope` available in `libs/core/` with full unit coverage.
- `ScopeManager` installed in `main.cpp`; Modal/Menu paths push/pop scopes correctly.
- `CommandRegistry` backs `KCommandBar` palette; `checkCallback` variant filters available commands.
- Vault switch destroys MainWindow and spawns a new one with the target vault; the known crash (`MEMORY.md / project_vault_switching.md`) is resolved.
- `hotkeys.json` round-trips byte-identically (Cluster B Phase 2 already did this; this phase is the command→hotkey wiring).
- No regressions in existing action/shortcut paths.

## Blocks / enables

- **Depends on:** Cluster B Phase 1 (DataAdapter for hotkeys.json write), Cluster B Phase 2 (VaultConfig for hotkeys.json load).
- **Blocks:** Cluster H (Menu scope-push depends on Scope), Cluster G (ViewRegistry factories return Components), any plugin-system work — essentially every P3 gap.
- **Enables:** `Modal`/`SuggestModal` subclassing, plugin command registration, per-vault MainWindow lifecycle cleanliness.
- **Estimated effort:** 2–3 weeks. Phase 1 is short (~3 days); Phases 2–4 each ~1 week.

## Preserved Obsidian compat quirks (do not silently "fix")

- `addRibbonIcon` keys on `title`, not `icon` — two same-title icons collide on the ribbon map. Preserve.
- `addCommand` mutates `cmd.id` in place; a second call double-namespaces (`pluginId:pluginId:origId`). Preserve.
- Status-bar plugin-id sanitiser regex lacks `/g` — only first illegal char is rewritten. Preserve.
- `Scope` child-first lookup — a child binding for `Ctrl+S` masks the parent's `Ctrl+S`, even if the child doesn't handle the key meaningfully. Preserve.

Document each in `docs/compat-quirks.md` with rationale on landing.
