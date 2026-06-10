# Cluster B — Plugin API surface completion (design)

> Spec for the post-reset Cluster B. Brainstormed 2026-04-28. Closes the 16-item gap between the Obsidian plugin-registration verb set and what Corbomite's plugin host exposes today. Pulls in `Vault.raw` + `Vault.config-changed` events from closed Cluster A.

## Goal

Every Obsidian registration verb that has a sensible KDE/Qt analogue is reachable from a third-party plugin via the existing plugin proxy facade. Permission tokens move into a public header and are documented. The `.obsidian/` watcher fires plugin events (`raw`, `config-changed`, `data.json`) on external edits.

## Scope (16 items)

| # | Verb / capability | Type | Permission |
|---|---|---|---|
| 1 | `registerHoverLinkSource` | Proxy over existing `HoverLinkSourceRegistry` | `ui.rendering` |
| 2 | `registerEditorSuggest` | Proxy over existing `EditorSuggestManager` | `ui.editor` |
| 3 | `registerMarkdownPostProcessor` | Proxy over existing `PostProcessorRegistry` | `ui.rendering` |
| 4 | `addRibbonIcon` | Proxy over existing `RibbonToolBar` | `ui.commands` (existing) |
| 5 | `registerEmbed` | Proxy over existing `EmbedRegistry` | `ui.rendering` |
| 6 | `registerMarkdownCodeBlockProcessor` | Proxy over existing `CodeBlockProcessorRegistry` | `ui.rendering` |
| 7 | `addStatusBarItem` | New `StatusBarRegistry` + proxy | `ui.statusbar` (new) |
| 8 | `registerObsidianProtocolHandler` | New `ProtocolHandlerRegistry` + proxy | `protocol` (new) |
| 9 | `registerEditorExtension` (decoration-only) | New `DecorationProviderRegistry` + proxy + 1 markoff hook | `ui.editor` (new) |
| 10 | `MarkdownRenderer.render` | Static API on Corbomite-side wrapper, returns `QFuture<void>` | none (pure render) |
| 11 | `addIcon` / Lucide icon registry | New `LucideIconRegistry` + proxy | `ui.icons` (new) |
| 12 | `data.json` external-edit watcher → `onExternalSettingsChange()` | Watcher expansion + lifecycle hook on `Plugin` | `config` (existing) |
| 13 | Move permission tokens to `corbomite/core/PluginPermissions.h` | Refactor | n/a |
| 14 | Document permission model in `docs/plugin-development/` | Docs | n/a |
| 15 | `Vault.raw` event | New signal on `Vault`; emitted from extended watcher | `vault.events` (existing) |
| 16 | `Vault.config-changed` event | New signal on `Vault`; emitted from extended watcher | `vault.events` (existing) |

## Architecture

### Proxy pattern (unchanged)

Every new verb follows the existing `CommandRegistrar`-style pattern:

1. **Host registry** owned by `MainWindow` / `Vault` / a singleton; lifetime spans the app or the vault.
2. **`PluginContext` lazily creates** the proxy/registrar on first plugin access, gated on a permission token.
3. **Registrar** prefixes registered ids with `<pluginId>:`, tracks them in a `QList<QString>`, and removes them on destruction.
4. **Plugin facade** (`Plugin::registerHoverLinkSource(...)` etc.) calls through `context()->hoverLinkSources()`.

This pattern is in place for `Command`, `View`, `MenuInjector`, `WorkspaceController`, `SecretStorage`, `ProcessSpawner`, `VaultProxy`, `FileManagerProxy`, `SearchProxy`, `MetadataCacheReader`. Cluster B adds 11 more registrars / proxies in the same shape.

### Permission tokens — new header, hybrid granularity

Today: `libs/vault/src/PluginContext.cpp:21-32` — file-private `constexpr auto` constants.

Move to: `libs/core/include/corbomite/core/PluginPermissions.h` — `inline constexpr auto` constants in `Corbomite::Permissions::` namespace. `PluginContext.cpp` includes the header; manifest declarations reference the constants. Plugin authors get a documented, IDE-completable list.

New tokens (5):
- `ui.rendering` — register popover/embed/post-processor/code-block-processor (#1, #3, #5, #6)
- `ui.editor` — register editor-suggest, decoration-provider (#2, #9)
- `ui.statusbar` — add status bar widget (#7)
- `ui.icons` — register Lucide icons (#11)
- `protocol` — register protocol handlers (#8)

`#10 MarkdownRenderer.render` is permissionless — it's a pure render call, no side effects. `#12 onExternalSettingsChange` rides the existing `config` token. `#15`, `#16` ride the existing `vault.events` token.

### `.obsidian/` watcher expansion

Today: `libs/vault/src/Watcher.cpp:28-39` excludes `.obsidian/` to avoid churn from Corbomite's own writes.

After Cluster B: watcher includes `.obsidian/`. Echo-suppression extends to `.obsidian/` paths via the existing `stampSelfWrite(rel, mtimeMs)` ledger — every `Vault::writeConfigJson` (and the new `VaultConfig::writeObsidianStyle`) stamps the path before writing. External edits (xdg-open editing `data.json`, Obsidian running concurrently, git checkout) bypass the ledger and fire events.

Three plugin-facing signals derive from the expanded watcher:
- `Vault::raw(QString relPath)` — fires for every external mutation (vault file or `.obsidian/`)
- `Vault::configChanged(QString relPath)` — fires for `.obsidian/*.json` external edits only
- Per-plugin `Plugin::onExternalSettingsChange()` virtual — fires when the plugin's own `data.json` is externally edited (via `PluginManager` watching the per-plugin path).

### Protocol scheme registration

Default: `corbomite://` registered with xdg-mime at install time (handled by `corbomite_add_plugin` + a one-time first-run `KIO::ApplicationLauncherJob`-equivalent or `xdg-mime default` shell-out — TBD in implementation).

Optional: Settings checkbox (`Settings → General → "Open obsidian:// links in Corbomite"`) opts in to also registering `obsidian://`. Both schemes funnel through the same `ProtocolHandlerRegistry` so plugin's `registerObsidianProtocolHandler("foo")` listens to `corbomite://foo` (always) and `obsidian://foo` (if opt-in is on).

xdg-mime registration writes to `~/.local/share/applications/mimeapps.list` (or KDE-equivalent). On uninstall, Corbomite restores the prior handler. Best-effort — failures non-fatal.

### `MarkdownRenderer.render` shape

Signature:
```cpp
namespace Corbomite::MarkdownRenderer {
    QFuture<void> render(
        Vault *vault,
        const QString &markdown,
        QWidget *parent,
        const QString &sourcePath,
        QObject *lifetime);
}
```

Internally instantiates a `Markoff::ReadingView` (the existing read-mode renderer), parents it to `parent`, ties its lifetime to `lifetime` via `QObject::deleteLater()` semantics, and returns a future that resolves when:
- Tree-sitter parse completes (sync today, but represented as future)
- All math `MathRenderItem`s have signaled `rendered()`
- All mermaid `MermaidRenderItem`s have signaled `rendered()`
- All embed/transclusion render-children have completed

Future is cancelled if `lifetime` is destroyed first. No permission required.

### `registerEditorExtension` — decoration-only hook

Plugin registers a `Corbomite::DecorationProvider`:
```cpp
class DecorationProvider : public QObject {
public:
    virtual QList<Decoration> produceDecorations(
        const QString &sourcePath,
        Markoff::Document *doc) = 0;
};
```

Markoff's render path (one new virtual hook in `Markoff::Editor::buildScene` / `ReadingView::buildScene`) iterates `Corbomite::DecorationProviderRegistry::instance().providers()` and merges the returned `Decoration`s with built-in tree-sitter-derived ones.

`Decoration` is a small POD: `{ start, end, kind, payload }` where `kind` is one of `{Highlight, InlineWidget, HoverBadge}`. Sufficient for the 80% case (highlight syntax, badge backlinks, inline icons).

**Deferred to Cluster E** (named explicitly):
- Gutter widgets (line-anchored UI in the source margin)
- Keymap injection via plugin (today: plugins use `addCommand` + the future hotkeys page)
- Theme overrides via plugin (today: plugins read the existing `ThemeService`)
- Custom cursor / selection rendering
- Multi-cursor support in Live mode
- Full `Markoff::EditorExtension` abstract base class

When Cluster E lands, `DecorationProvider` will become one of several `EditorExtension`-attached registries — additive, not breaking. The verb name `registerEditorExtension` was chosen deliberately so plugin authors writing against the Cluster B surface won't have to migrate.

## Phase breakdown

Four phases, gated. Each phase ends in a green test run + a single squashed commit.

### Phase 1 — Mechanical proxies (#1, #2, #3, #4, #5, #6)

Six proxies over existing host registries. No host-side changes (registries already exist). Each proxy gets ~80 lines (.h + .cpp) following the `CommandRegistrar` template. `PluginContext` gets six new lazy accessors.

Test fixture: a stub `Plugin` subclass per registry registers, asserts the registry sees it, destroys, asserts the registry no longer sees it.

### Phase 2 — New host-side surfaces (#7, #11, #10, #9)

Four new registries, each with a host-side substrate:
- `StatusBarRegistry` — wired into `MainWindow::statusBar()` (existing). Each registered item gets a slot via `addPermanentWidget`.
- `LucideIconRegistry` — singleton mapping `lucide-*` names to `QIcon`s. Bundle a curated subset of Lucide SVGs at `data/icons/lucide/` (~100 icons, the ones Obsidian's built-in commands reference). Plugin's `addIcon(name, svg)` registers ad-hoc additions.
- `MarkdownRenderer::render` — pure function in `corbomite/core/MarkdownRenderer.{h,cpp}`. Wraps `Markoff::ReadingView`. Returns `QFuture<void>`. Tests: render trivial markdown into a `QWidget`, await the future, assert text is present.
- `DecorationProviderRegistry` + the markoff hook. Smallest markoff change: one virtual call in the decoration build path.

Markoff change scope: a single PR to `libs/markoff-family/libs/markoff` adding `Markoff::DecorationProviderHook` interface + one virtual call site. Submodule pin bump in this cluster.

### Phase 3 — Lifecycle / events (#8, #12, #15, #16)

- `Vault::raw(QString)` and `Vault::configChanged(QString)` signals; watcher expansion to `.obsidian/`; ledger extension.
- `ProtocolHandlerRegistry` + xdg-mime registration helper. URL parser handling `obsidian://` and `corbomite://` schemes uniformly.
- `Plugin::onExternalSettingsChange()` virtual; `PluginManager` watches per-plugin `data.json` paths and invokes the hook on change.

Test fixtures use the existing `MemoryAdapter` to inject .obsidian/ writes; assertions on signal emissions + virtual hook invocations.

### Phase 4 — Permissions polish (#13, #14)

- Move tokens to `corbomite/core/PluginPermissions.h`.
- Replace string-literal permission checks throughout `PluginContext.cpp` with `Permissions::kVaultRead` etc.
- Add `docs/plugin-development/permissions.md` documenting each token: what it gates, what plugins use it, when to deny.
- Update `corbomite_add_plugin` CMake helper docstring to reference the new header.

## Out of scope (named, deferred)

- **Full `EditorExtension` ABI** — Cluster E. See "registerEditorExtension — decoration-only hook" above for the named deferred items.
- **JS sandbox / WebEngine plugin loader** — far-future. The audit's `PLUGIN-API-SKETCH.md` §1 notes this is the long-term path for binary-compat with Obsidian; out of scope for any current cluster.
- **Plugin marketplace UI** — separate concern; would couple to KPluginInstaller.
- **Multi-cursor in Live mode** — Cluster E.
- **`MarkdownRenderChild` framework** — punch-list P3. Decoration-only hook covers the related need; the parent registry / addChild / mount/unmount signals are a separate refactor.
- **`addStatusBarItem` interaction model** (clickable items, context menus) — Phase 2 ships passive widgets; click handling is a punch-list follow-up if a real plugin needs it.
- **Lucide icon updates after registry initialisation** — bundled set is fixed at app start; plugins can `addIcon` for ad-hoc additions, but bulk theme switching of the bundled set is out of scope.

## Testing strategy

- **Per-proxy unit tests** following the `tst_command_registrar` pattern: instantiate registry, register from a stub plugin, assert visibility, destroy stub, assert cleanup.
- **Watcher expansion test** in `tst_vault_watcher`: write to `.obsidian/data.json` externally, assert `raw` + `configChanged` fire; write via `Vault::writeConfigJson`, assert echo-suppression (no signal).
- **`MarkdownRenderer::render` test** using `QFutureSynchronizer`: render markdown with math, await, assert math item rendered.
- **Protocol-handler test** using `QDesktopServices::openUrl` simulation (without actually invoking xdg-mime): plugin registers handler, simulated URL is dispatched.
- **Permissions test**: stub plugin without a token tries to register, assertion fails; with token, succeeds.
- **End-to-end**: a "kitchen-sink" test plugin under `tests/plugins/cluster-b-kitchen-sink/` exercises every new verb in one `onload`. Used as the canonical reference plugin for documentation.

## Risks / open questions

- **xdg-mime mechanics on non-KDE desktops**: GNOME / wlroots / sway have their own MIME machinery. Best-effort flag covers this — first-run dialog says "couldn't register protocol; you may need to do this manually."
- **Markoff submodule pin bump in Phase 2** — coordinate with any in-flight Markoff work (Cluster G inline-ORC). Likely no conflict; the decoration hook is a small addition.
- **Lucide icon bundling size** — 100 SVGs is ~50 KB. Acceptable; ship as Qt resource.
- **`Vault.raw` rate** — every `.obsidian/` mutation fires the signal. Heavy-write plugins (e.g., one writing `data.json` on every cursor move) will spam; document as plugin-author footgun.
- **`onExternalSettingsChange` debounce** — first revision fires immediately on watcher signal. If plugins complain, add a 100ms debounce.
- **Dynamic plugin reload + status-bar leftovers** — verify `StatusBarRegistry` cleanup actually removes the widget from `QMainWindow::statusBar()`. Easy to test; flag if widgetless-removal behaves oddly in Qt 6.11.
