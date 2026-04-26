# Core + addenda audit

Audit date: 2026-04-26.
Spec inputs: `docs/obsidian-audit/domains/core.md` (Pass 2), the five addenda
listed in the task brief, and the original Obsidian sources under
`~/bin/ObsidianRAW/audit/renamed/obsidian/`.

## App / service registry parity

Obsidian collapses every cross-cutting service onto `App` (`App.js:10-40` for
boot-minimal, `:515-555` for vault-dependent) and every consumer reaches
siblings via `this.app.<field>`. Corbomite has deliberately *not* cloned this
god object — `core.md §11` recommends not doing so — and instead splits the
role across two classes:

- `Corbomite::CorbomiteApp` (`src/app/CorbomiteApp.h:26-54`,
  `src/app/CorbomiteApp.cpp:10-65`) owns process-wide state. As of Q.0 it
  holds only `m_currentPath`, a `RecentVaults` helper, and a `PluginManager *`.
  Its sole `vault*` API is the path string + a pair of `vaultOpened` /
  `vaultClosed` signals. **It owns no canonical Vault, MetadataCache,
  Workspace, CommandRegistry, or settings.**
- `Corbomite::MainWindow` (`src/app/MainWindow.h:73-235`) owns *every*
  vault-scoped service: `m_vaultObj`, `m_fileManager`, `m_workspace`,
  `m_viewRegistry`, `m_searchIndex`, `m_metadataCache`, `m_linkResolver`,
  `m_templateService`, `m_dailyNoteService`, `m_commandRegistry`,
  `m_menuEvents`, `m_hoverSources`, `m_hoverPopover`, `m_themeService`,
  `m_embedRegistry` + `m_embedRenderer`, `m_suggestManager`, two suggesters,
  and the ribbon. Plugin contexts pull from MainWindow via
  `MainWindow::rewirePluginCoreServices()` (`src/app/MainWindow.cpp:833`,
  called both at startup and on every vault open at line 2134).

Coverage of the field catalogue (`core.md §2`):

| Obsidian `App` field | Corbomite owner | Status |
|---|---|---|
| `vault` | `MainWindow::m_vaultObj` (`Corbomite::Vault`) | Present |
| `metadataCache` | `MainWindow::m_metadataCache` | Present |
| `workspace` | `MainWindow::m_workspace` | Present |
| `fileManager` | `MainWindow::m_fileManager` | Present |
| `viewRegistry` | `MainWindow::m_viewRegistry` | Present |
| `embedRegistry` | `MainWindow::m_embedRegistry` (concrete `Markoff::EmbedRegistry`) | Present |
| `commands` | `MainWindow::m_commandRegistry` | Present |
| `hotkeyManager` | folded into `KActionCollection` + KConfig | Partial — no `.obsidian/hotkeys.json` reader |
| `dragManager` | none | **Missing** |
| `customCss` | none | **Missing** (theme-service only) |
| `renderContext` | none equivalent (per-leaf engines) | **Missing** |
| `secretStorage` | per-plugin `Corbomite::SecretStorage` proxy (`libs/core/src/proxies/SecretStorage.cpp`) | Present |
| `plugins` | `CorbomiteApp::m_pluginManager` | Present |
| `internalPlugins` | also `m_pluginManager` (no formal split) | Partial — see §"Plugin-API version gating" |
| `setting` | `SettingsDialog` instantiated on demand at `MainWindow.cpp:1106` | Partial |
| `appId` | none | **Missing** (no per-vault process namespace) |
| `keymap` | `Corbomite::ScopeManager` (singleton, `src/app/main.cpp:41`) | Partial |
| `scope` (root) | implicit in `ScopeManager::m_stack` | Partial |
| `secretStorage` (vault-aware) | proxy shape ignores vault scope (uses pluginId only) | Notable concern (see §"Secrets") |

The Pass 2 doc explicitly approves not cloning the god object. The split is
internally consistent: `CorbomiteApp` is a *vault-lifecycle* signaller,
`MainWindow` is the runtime registry. The biggest structural gap relative to
`core.md §10` is `dragManager` — no centralized drag coordinator exists.
Drag-and-drop is delegated to KDDockWidgets (tab moves) and individual widgets
(file-tree drag). For an Obsidian-faithful plugin API, plugins would not have
a way to (e.g.) listen for "a file from the explorer was dropped onto a
markdown editor" — they'd need to install per-target event filters.

Note also that `MainWindow` is a `CorbomiteMDI::MainWindow` subclass, so the
docking substrate (KDDockWidgets MainWindow) lives one layer further down.
There is no `App.dom.{appContainerEl, workspaceEl, statusBarEl}` analogue —
the closest is `MainWindow::m_centralStack` (`MainWindow.h:196`) plus the
KDDockWidgets layout area, accessed implicitly through `Workspace` APIs.

## Per-vault isolation risk (cross-vault state leaks)

This is the highest-priority finding. Obsidian's invariant (`core.md §2,
"Vault-switch behaviour"`) is that *no field survives a vault switch* — the
process dies and a new window comes up. Corbomite cannot do that (KDE-app
expectation is in-process vault swap).

`MainWindow::onVaultClosed` (`src/app/MainWindow.cpp:2190-2263`) tries to
correctly reset state. It tears down — in declared order —
`m_autosave`, `m_sessionManager`, `m_templateService`, `m_dailyNoteService`,
clears `m_wikiSuggest::setVault(nullptr)`, `m_tagSuggest::setIndex(nullptr)`,
`m_hoverPopover::setVault(nullptr)`, `m_embedRenderer::setMetadataCache /
setResources(nullptr)`, `m_popoverResources.reset()`, then `close()` +
`delete` of `m_metadataCache`, `m_fileManager`, `m_vaultObj`, `m_searchIndex`,
`m_linkResolver`. Plugins are torn down first via
`PluginManager::disablePlugin(id, persist=false)` so their per-vault
`PluginContext` proxies expire before the services they wrap.

`onVaultOpened` (`MainWindow.cpp:1941-2188`) re-creates the same nine
services. It does the right thing — `delete m_X; m_X = new X(...)` for each
one — and re-runs `rewirePluginCoreServices()` so plugins receive fresh
proxies. The pattern is correct.

**However, several stale references can survive a vault switch:**

1. **`m_fsAdapter` is constructed once and never reset**
   (`MainWindow.cpp:1957`: `if (!m_fsAdapter) m_fsAdapter = std::make_unique<FileSystemAdapter>();`).
   This is fine *if* `FileSystemAdapter` is genuinely vault-agnostic (looks
   like it is — the path goes in via `Vault::load`), but the comment in
   `MainWindow.h:173` (`Q.0 P6 — canonical Vault aggregate created alongside
   the legacy VaultModel`) calls it out as a known long-lived holder.

2. **`m_embedRegistry`'s factories are registered once at construction and
   never reset** (see `MainWindow.h:208-211`, "Built once at MainWindow
   construction; the per-vault resource adapter is rebuilt on each
   `onVaultOpened` and released on `onVaultClosed`"). Internal embed factories
   that close over `Vault*` or `MetadataCache*` would leak. The current
   factories close over the *adapter*, which is rebuilt — so this is OK.
   But any plugin that registers an embed factory at `onload` will have its
   factory survive a vault switch with stale captures, since the registry
   itself is not rebuilt and plugins are reloaded. **This is a latent
   plugin-API leak.**

3. **`m_viewRegistry::setFileResolver(...)`** sets the resolver to capture
   `[this]` referencing `m_vaultObj`. On `onVaultClosed` this is correctly
   reset to `nullptr` (`MainWindow.cpp:2216-2217`). OK.

4. **`m_themeService`** is constructed once with a `KColorSchemeManager`
   and never destroyed across vault switches. This is correct — themes are a
   per-host, not per-vault, concept (Obsidian stores theme name in
   `appearance.json` which is per-vault, but the rendering pipeline is global).

5. **`m_hoverPopover`, `m_wikiSuggest`, `m_tagSuggest`, `m_suggestManager`,
   `m_ribbonToolBar`** are all constructed once and have explicit
   `setVault(nullptr)` / `setIndex(nullptr)` resets in `onVaultClosed`
   (`MainWindow.cpp:2230-2232`). These are correct provided the setters
   actually drop their internal pointers — verify per setter; the audit
   didn't follow each one.

6. **Adapter trio (`m_linkResolverAdapter`, `m_metadataCacheAdapter`,
   `m_metadataParserImpl`)** are `std::unique_ptr`, recreated in
   `onVaultOpened` (`MainWindow.cpp:2000-2008`), but `onVaultClosed` does
   *not* reset them. `m_embedRenderer->setMetadataCache(nullptr)` correctly
   nullifies the inbound pointer, but the adapters themselves live on holding
   `m_linkResolver` / `m_metadataCache` pointers that have been deleted. If
   anything still holds a copy of an adapter pointer between
   `onVaultClosed` and the next `onVaultOpened`, a use-after-free is on the
   table. **This is a real leak window** — small (closeVault → openVault is
   typically one event-loop tick), but real, and exactly the smell flagged in
   the brief.

7. **`m_popoverResources`** is correctly `.reset()` (`MainWindow.cpp:2239`).

8. **`m_recentVaults` / `RecentVaults` / KConfig state** is global; correct.

The `onVaultOpened` path on line 1958 — `delete m_vaultObj; m_vaultObj = new
Vault(...)` — assumes the previous `m_vaultObj` had an in-flight callback that
stored a raw `m_vaultObj` pointer; if so, the second `Vault::load` could
arrive after the previous-vault async callback re-enters MainWindow with a
stale pointer. Q.0's signal-based echo-suppression in `Vault` reduces this
risk (per memory `project_cluster_q0_done.md`), but the audit cannot prove
absence.

**Recommendation:** in `onVaultClosed`, also reset the three adapters
(`m_linkResolverAdapter.reset(); m_metadataCacheAdapter.reset();
m_metadataParserImpl.reset();`) before deleting `m_metadataCache` /
`m_linkResolver`. This costs nothing and closes the only window where the
audit found a real dangling pointer.

## Events mixin → Qt signals translation

Corbomite ships a faithful `Corbomite::Events` reimplementation
(`libs/core/include/corbomite/core/Events.h:57-94`, impl at
`libs/core/src/Events.cpp:30-95`). Semantics are precisely matched:

- `EventRef` is an opaque `weak_ptr<Node>` (`Events.h:20-30`,
  `Events.cpp:17-22`). Stale-ref `offref` is a no-op (`Events.cpp:42-51`).
- `trigger` snapshots `byName[name]` before iterating (`Events.cpp:58`), so
  a self-`offref` from inside a listener doesn't shift iteration. Matches
  Obsidian's `:slice()` quirk (`core.md §1, Events.tryTrigger`).
- `tryTrigger` runs each listener in `try/catch` and reschedules the
  exception via `QTimer::singleShot(0, [eptr]{ rethrow_exception(eptr); })`
  (`Events.cpp:73-86`). This is the exact `setTimeout(…, 0)` semantics
  `core.md §1` calls for, and the comment correctly notes
  `Qt::QueuedConnection` would silently swallow the exception.
- `pendingAsyncRethrows()` test hook (`Events.cpp:90-93`) is a Corbomite
  addition for Cluster C tests, not present in Obsidian. Harmless.

`off(name, fn)` (Obsidian's identity-match) is **not provided**: only
`offref` exists. Per the header comment (`Events.h:43-46`: *"Not typically
useful for lambdas; prefer offref()"*), this is intentional. If a third-party
plugin tries to translate `app.workspace.off("file-open", myFn)` literally
they will get a compile error rather than the Obsidian collision footgun.
**Compatibility fence, not a bug.**

`Events` is *not* mixed into Workspace, Vault, MetadataCache, ViewRegistry
the way Obsidian does. Each Corbomite emitter uses Qt signals: see
`Workspace.cpp:116, 153, 158, 188, 197, 253, 261, 289, 317, 324, 345, 365,
503, 510, 511, 522, 523, 841, 905`. So Corbomite has two parallel pub-sub
systems. Plugin code reaching `app.workspace.on("layout-change")` would need
a translation layer that maps the Obsidian event name onto the right
QObject + Qt signal. As of 2026-04-26, no such mapping table exists.

**App-level event emission audit:**

- `css-change` — Obsidian `App.js:1293, 2808, 2883, 2896` emits this on
  `app.workspace`. Corbomite emits **nothing** equivalent. `m_themeService`
  has a `themeChanged(const Markoff::Theme &)` signal
  (`libs/core/include/corbomite/core/ThemeService.h:42`), and it fires
  on `setActiveThemeByName`/`refreshSystemTheme`, but no one re-broadcasts
  this onto `Workspace` as `css-change`. **Plugins observing theme changes
  would have to subscribe to `m_themeService` directly — not currently
  exposed via the proxy surface.** Filing as a top concern.
- `quit` — Obsidian `App.js:3219`, payload is the `Eb` quit-tasks collector.
  Corbomite handles close in `MainWindow::closeEvent`
  (`MainWindow.cpp:393-403`), which calls `confirmCloseUnsaved()` then
  `saveSessionState()`. There is **no equivalent of the `Eb` collector** —
  plugins cannot register async cleanup futures. This means a plugin doing
  background work has no opportunity to flush before window close.
  Currently invisible because no plugin needs it; will bite when one does.
  Filing as a top concern.

## Scope → KActionCollection mapping

`Corbomite::Scope` (`libs/core/include/corbomite/core/Scope.h:48-94`,
impl at `libs/core/src/Scope.cpp:6-61`) is a faithful port of the Obsidian
class:

- `registerBinding(modifiers, key, Handler)` returns a `KeyBinding` opaque
  handle backed by `weak_ptr<Node>` (`Scope.h:17-27`, `Scope.cpp:8-25`).
- `handleKey(QKeyEvent*)` walks the same `(mods, key)` slot — a
  `QHash<Key, QList<...>>` keyed on `(Qt::KeyboardModifiers, int)` — then
  delegates to `m_parent->handleKey` (`Scope.cpp:38-59`). Multiple bindings
  with the same `(mods, key)` coexist and are tried in registration order
  (`Scope.cpp:48-53`). Matches Obsidian's `:27` semantics.

The notable departure is *return convention*. Obsidian's `KeyHandler.func`
uses three-valued (`false` = consume + preventDefault, `undefined` = bubble,
truthy = consume silently); Corbomite's `Handler` returns `bool` (`true` =
consume, `false` = bubble). The "catch-all returning undefined keeps
walking" Obsidian quirk (`core.md §8, Scope.handleKey bubbles to parent
only for catch-all handlers returning undefined`) is **not implemented** —
Corbomite always falls through to the parent on `false` regardless of
whether the binding was a catch-all. For internal use this is fine; it
diverges from Obsidian only in an edge case (multi-handler same-slot
return-undefined chains), but plugins that ported their `Scope.register`
verbatim would see different behaviour.

**Scope stack management** lives in `Corbomite::ScopeManager`
(`libs/core/src/ScopeManager.cpp:13-99`):

- Singleton, installed as global event filter via
  `installOnApplication()` (`ScopeManager.cpp:25-34`); main wires it at
  `src/app/main.cpp:41`.
- `pushScope(s)` / `popScope()` / `removeScope(s)` (`ScopeManager.cpp:36-50`)
  — LIFO stack matching Obsidian's `Keymap.pushScope`/`popScope`.
- `dispatchKey` walks **top-down** (`ScopeManager.cpp:76-80`: "First
  consumed wins"). Obsidian conceptually walks the active scope only and
  bubbles via `Scope.parent`. The Corbomite design is different (active
  scope chain is implicit in stack order, not via `parent` pointer). Both
  produce equivalent results for shallow stacks.
- `defaultBypass` predicate (`ScopeManager.cpp:57-64`) lets `QLineEdit`,
  `QTextEdit`, `QPlainTextEdit` swallow keystrokes before scope dispatch.
  Matches Obsidian's input-element exception. Customisable via
  `setBypassPredicate`.
- `setTabFocusContainerEl` from Obsidian's `Scope`: **not implemented**.
  Modal tab-trap (focus-out → focus-in re-cycle) would need a per-scope
  hook. None of Corbomite's modals use this today.

**Real Scope users:** `grep -rn "ScopeManager::instance\|new Scope("` finds
**zero callers** outside `ScopeManager.cpp` itself and `main.cpp`. The
infrastructure exists but no `Modal`, `Menu`, `EditorSuggest`, view, or
plugin pushes onto it. Effectively, the global scope stack is always
empty, all keys bypass it, and `KActionCollection` (via Qt's normal
shortcut routing) handles every hotkey. This is fine today (no plugin needs
modal hotkey containment), but the moment a plugin spawns a `Modal` and
expects `Esc` to close *only* the modal without triggering the editor's
`Esc`, the existing infrastructure will not save them.

## Drag manager / Embed registry

**Drag manager:** absent. `grep -rn "DragManager"` returns zero hits in
`libs/core/`, `libs/vault/`, `src/`. The closest substitutes are
KDDockWidgets' tab-drag (host-managed) and per-widget `dragEnterEvent` /
`dropEvent` handlers in the file-tree and editor. There is no event for
"plugin wants to coordinate a custom drag mime type globally", no central
drag-state observation surface, and no way to listen to drags on a tree
without owning the widget. Plan-side gap; not currently surfaced as a bug.

**Embed registry:** present and faithful.
`Markoff::EmbedRegistry` (`libs/markoff-family/libs/markoff-core/include/markoff/EmbedRegistry.h:29-52`)
maps file extensions to factories; `MainWindow::m_embedRegistry`
(`MainWindow.h:210`) is the canonical instance per the C4 Task 13 note in
the header comment. `m_embedRenderer` (`Markoff::Reading::EmbedRenderer`)
plus `m_popoverResources` (rebuilt per vault) drive `![[image.png]]`
rendering and the hover popover preview. Cluster J-style internal-only
registries; not yet exposed via plugin API per the
`feedback_plugin_api_stability` memory note.

The lifetime concern flagged above ("factories registered at MainWindow
construction outlive vault switches") applies — a plugin registering an
embed factory that closed over `m_vaultObj` would dangle. Fix is for plugin
embed factories to capture by-id and look up via the proxy at dispatch time.

## Vault-format-critical utils: resolveSubpath / stripHeading round-trip

These two functions are the load-bearing primitives for `[[Note#Heading]]`
and `[[Note#^blockid]]` link resolution. **Obsidian-vs-Corbomite divergence
here would silently break links across vaults.**

### `stripHeading`

Obsidian (`utils/stripHeading.js:5-7`):
```
e.replace(AT, " ").replace(/\s+/g, " ").trim();
AT = /[!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\r\n]/g
```

Corbomite (`libs/core/src/LinkUtils.cpp:69-74`, `:12-17`):
```
re = QRegularExpression("[!\"#$%&()*+,.:;<=>?@^`{|}~/\\[\\]\\\\\\r\\n]")
collapseAndTrim — replaces \s+ with " " and trims
```

**Identical character class. Identical algorithm.** The Corbomite regex
escapes `[`, `]`, `\` and `/` for QRegularExpression; the unescaped
character set is `!"#$%&()*+,.:;<=>?@^`{|}~/[]\` plus `\r\n` — the same
13 characters Obsidian strips. No drift.

### `stripHeadingForLink`

Obsidian (`utils/stripHeadingForLink.js:5-7`):
```
e.replace(PT, " ").replace(/\s+/g, " ").trim();
PT = /([:#|^\\\r\n]|%%|\[\[|]])/g
```

Note Obsidian's `]]` is written as `]]` (the `]` is the closing alternation
bracket; the inner `]` doesn't need escaping in JS regex when it's not
the very first char of a group — see `_internal.js:181753`).

Corbomite (`LinkUtils.cpp:76-81`, `:21-26`):
```
re = QRegularExpression("([:#|^\\\\\\r\\n]|%%|\\[\\[|\\]\\])")
```

Decoded character class: `[:#|^\\r\n]` plus alternates `%%`, `[[`, `]]`.
**Matches Obsidian exactly.** OK.

### `resolveSubpath`

Obsidian's algorithm (`utils/resolveSubpath.js:5-79`):
1. Split subpath by `#`, drop empties.
2. If only one segment:
   - `^blockid` → look up in `cache.blocks` (case-insensitively, via two
     loops) + correlate with `cache.listItems[].id` for the list-item
     payload.
   - `[^footnote]` → loop `cache.footnotes` for matching `id`.
3. Otherwise walk `cache.headings` doing a multi-segment heading walk —
   `stripHeading(heading.heading).toLowerCase() === stripHeading(segment).toLowerCase()`,
   incrementing through segments, requiring strictly-deeper headings between
   matched segments, terminating on first segment-out-of-range or a sibling
   `level <= y`.

Corbomite (`LinkUtils.cpp:83-198`):
- **Footnote** (`[^id]`): scans `source` for `[^id]:`, builds a range to the
  next `\n\n`. Obsidian uses pre-parsed `cache.footnotes`; Corbomite re-scans
  source. Functionally equivalent for normal footnotes, but
  Corbomite's lookup is **literal-match** on the brackets while Obsidian's
  is `m.id === p` after slicing `[^...]` off — Corbomite would miss a
  case-mismatched footnote id Obsidian would still find (Obsidian's `===`
  is case-sensitive too, so this is actually fine).
- **Block** (`^blockid`): scans `source` for the literal marker string
  (e.g. `^myblock`). Returns the surrounding paragraph by walking line
  starts. Obsidian uses `cache.blocks[id]` lookup, **case-insensitive**
  (`r = i.substr(1).toLowerCase()`, then iterates blocks comparing
  `a.toLowerCase() === r`). **Corbomite is case-sensitive on block ids; this
  diverges from Obsidian.** A vault saved with `^MyBlock` defined and
  `[[Note#^myblock]]` referenced would resolve in Obsidian and fail in
  Corbomite. Filing as a top concern.
- **Heading**: Walks `Markoff::HeadingInfo` from the parsed document; matches
  the *trailing* segment of a `/`-joined subpath (`stripHeading(fragment).
  toLower() != needle`). Obsidian does a multi-segment walk requiring each
  intermediate path component to match a properly-nested ancestor. **Corbomite
  matches only the last segment**, which means `[[Note#H1/H2/H3]]` resolves
  on the first `H3` regardless of whether it nests under `H1 > H2`. This is
  a real behavioural divergence. The comment on line 167-168 acknowledges
  it: "Obsidian dispatches segment-by-segment on `/`-joined paths; we match
  the trailing segment." Filing as a top concern (low-impact in practice
  since multi-segment heading subpaths are rare, but vault-portability says
  match Obsidian).
- The block-end computation correctly stops at the next same-level-or-higher
  heading via the inner loop (`LinkUtils.cpp:181-188`). Matches Obsidian's
  `next` / `w` wiring.
- Obsidian returns the **list item** payload alongside the block when a
  block id refers to a list item (`resolveSubpath.js:21-28`). Corbomite
  returns only the block range — no list-item correlation. Plugins reading
  the resolution would not be able to detect "this block is a list item"
  from `SubpathResolution`. Minor; not currently consumed.

**Round-trip risk:** the case-sensitive block-id miss is the highest-priority
divergence. Headings (multi-segment walk) is second. `stripHeading` /
`stripHeadingForLink` are byte-identical. Footnote handling is equivalent.

## Plugin-API version gating

Obsidian: `apiVersion`, `requireApiVersion(v)` — string semver against
build-time constant.

Corbomite: per memory `project_cluster_n_done`, ships
`X-Corbomite-MinVersion` and `X-Corbomite-ApiLevel`. Confirmed at
`libs/vault/include/corbomite/vault/PluginManager.h:39, 44`:

> `IncompatibleVersion` — plugin's declared X-Corbomite-MinVersion
> `IncompatibleApiLevel` — plugin's declared X-Corbomite-ApiLevel

Both checked at discovery time; mismatched plugins are loaded as inert
metadata. Two-axis gating (semver + integer level) is richer than
Obsidian's single-axis `apiVersion`. Surface is fine; cluster N closure
note in memory matches what's in the code.

## Date format token compat (moment.js → Qt)

`Corbomite::MomentFormatter::format(QDateTime, momentFormat, locale)`
(`libs/core/src/MomentFormatter.cpp:291-334`) is a hand-rolled three-pass
parser:

1. Pass 1 (`extractEscapes`, `:45-75`): pull `[literal text]` spans into
   sentinel placeholders `\x01<idx>\x02`. Unterminated `[` is treated as a
   literal `[`.
2. Pass 2 (`dispatchToken`, `:115-287`): longest-match-first walk over the
   sentinelized format string. Implemented tokens:
   - 4-char: `YYYY`, `MMMM`, `DDDD`, `dddd`.
   - 3-char: `MMM`, `DDD`, `ddd`, `SSS`.
   - 2-char: `YY`, `MM`, `DD`, `Do`, `dd` (2-letter short day), `ww`, `HH`,
     `hh`, `mm`, `ss`, `SS`.
   - 1-char: `M`, `D`, `d`, `w`, `H`, `h`, `m`, `s`, `S`, `a`, `A`, `X`, `x`.
3. Pass 3 (`restoreEscapes`, `:80-109`): swap sentinels back for literals.

**Coverage check vs Obsidian's documented Moment usage:**
- `YYYY-MM-DD` (default daily-note): supported.
- `[Year:] YYYY`: bracket escape — supported.
- `Do` ordinal suffix: supported, EN-only (`ordinalSuffix`, `:21-36`); other
  locales fall back to bare number. Comment notes "matches Obsidian's
  observed inconsistency for non-EN `Do`" — addendum-correct.
- `dddd` / `ddd` / `dd` / `d`: supported. Note `d` returns `0..6` (Sun=0)
  to match Moment, computed by `dt.date().dayOfWeek() % 7`
  (`MomentFormatter.cpp:236`).
- `X` / `x`: epoch seconds / ms — supported.
- **Not implemented:** `Y` (signed year), `Q` (quarter), `gg`/`gggg` (locale
  week year), `e`/`E` (locale day-of-week), `k`/`kk` (1-based hour), `Z`/`ZZ`
  (timezone offset), `LT`/`LTS`/`L`/`LL`/`LLL`/`LLLL`/`l`/`ll`/`lll`/`llll`
  (locale shortcut formats). These are rarer in user-typed daily-note
  formats but **plugins or vault templates that use them would render the
  literal characters** rather than the formatted output. For example a
  template containing `LT` would output the literal `LT` in Corbomite
  but `5:32 PM` in Obsidian.

The longest-match-first walk is the right approach (`MMMM` before `MMM`
before `MM` before `M`). The token table is the file's bulk; adding the
missing tokens is mechanical follow-up. Filing as a minor concern; vault
portability is not violated for any default Daily Notes / Templates settings.

## Platform feature flags

Obsidian's `Platform.{isDesktop, isMobile, isMacOS, isLinux,
canExportPdf, canPopoutWindow, canStackTabs, supportsIndexedDb}` provides
fan-out feature gates.

Corbomite's `Corbomite::Platform` namespace
(`libs/core/include/corbomite/core/Platform.h:6-29`) exposes only:
- `openWithDefaultApp(absolutePath)` — `QDesktopServices::openUrl` shim.
- `showInFolder(absolutePath)` — DBus FileManager1 → xdg-open fallback on
  Linux, `open -R` on macOS, `explorer /select,` on Windows.

**No feature-flag accessors exist.** No `isDesktop`, `isLinux`, `isMacOS`,
`canExportPdf` etc. The compile-time `Q_OS_*` defines are used inline in
implementation (`Platform.cpp:35, 38, 45`). For internal code this is
adequate (Corbomite is desktop-only). For a future plugin API, **plugins
have no way to ask "are popout windows available" or "can the host export
PDF"** without parsing platform headers themselves.

`canExportPdf` is somewhat present implicitly via `src/app/ExportToPdf.cpp`
existing as a host capability, but there's no boolean accessor.

Filing as a minor concern; will need attention when plugin API exposes
host-feature negotiation.

## Secrets storage parity

`Corbomite::SecretStorage`
(`libs/core/include/corbomite/core/proxies/SecretStorage.h`,
impl `libs/core/src/proxies/SecretStorage.cpp:53-167`) is a per-plugin
proxy keyed on `(pluginId, secretId)` strings.

- Backing store: Qt6 Keychain (gated by `CORBOMITE_HAVE_KEYRING` build
  define, lines 73-86, 100-115, 129-143). When unavailable falls back to a
  process-local `QHash<QString, QString>` guarded by `QMutex`. Fallback
  data is **not persisted** — explicit warning at lines 84-85.
- Key namespacing: `<pluginId>.<secretId>` (`fullKey`, `:42-49`). All
  pluginIds share the same `kKeyringService = "corbomite-plugin"` keyring
  service.
- Permission gating: `setSecret`, `getSecret`, `deleteSecret`, `listSecrets`
  all check `hasSecretsPermission()` first. Granted-by-default to single-arg
  ctor (legacy form, `:53-55`); explicit form takes a `QSet<QString>` of
  granted tokens.
- `listSecrets` (`:150-165`) only enumerates the in-process fallback —
  **the keyring backend is not enumerated**. A plugin that lost track of its
  own keys could not list the keyring entries it created. Documented Qt
  Keychain limitation; not a Corbomite bug.
- `isAvailable()`: not provided. Obsidian's `SecretStorage.isAvailable()`
  is the gate plugins use to decide whether to even try. Corbomite returns
  `false` from `setSecret` only after attempting and failing. Filing as
  minor concern.
- **No vault scoping.** Secrets are pluginId-scoped, not (pluginId, vaultId)
  scoped. The same plugin enabled in two vaults shares the same
  `corbomite-plugin/<pluginId>.<secretId>` keychain entry. Obsidian solves
  this via `appId` (per-vault namespace, `core.md §8`); Corbomite has no
  `appId`. **Probably correct for most plugins** (a secret is host-wide,
  not per-vault), but worth surfacing.

QtKeychain integration is solid. Single concern is the lack of `isAvailable`
and the no-vault-scoping consequence of having no `appId`.

## --- Addenda ---

## File Recovery

**Status: not implemented as a plugin.**

The closest thing in the tree is `libs/core/src/TextFileView.cpp:140`:

```
QString recoveryDir = m_vaultRoot + QStringLiteral("/.obsidian/file-recovery");
QDir().mkpath(recoveryDir);
QString backupPath = recoveryDir + "/" + baseName + "-" + timestamp + ".md";
```

This is the Cluster G save-failure backup path called out in the addendum's
§7 ("Compatibility note"): a **per-file `.md` snapshot in
`.obsidian/file-recovery/<filename>-<timestamp>.md`**, *not* the
Obsidian-format `.obsidian/file-recovery.json` single-file store with
`{version, maxSnapshotAge, maxSnapshotsPerFile, entries: {path: [{ts, data},
...]}}`.

Missing pieces (per addendum):
- No periodic snapshotting (only failure backups exist).
- No `maxSnapshotAge` / `maxSnapshotsPerFile` retention policy.
- No `{"failure": true}` discriminator on save-failure backups.
- No Version History modal.
- No `file-recovery:open` command.
- No restore-creates-new-snapshot behaviour (no restore at all).
- No "Open version history" hamburger menu entry (the addendum's §6 notes
  Cluster R was supposed to ship this as a disabled placeholder; the audit
  did not find such a placeholder in `MarkdownView` / `TextFileView`).

The Cluster G save-failure format is a divergent on-disk artefact that, per
the addendum's §7 recommendation, should be migrated into the JSON store
on first run when Cluster T ships. As of 2026-04-26, Cluster T is not
scheduled (per memory and PROJECT-STATE).

## Bookmarks core plugin

**Status: implemented and Obsidian-compatible on the core path.** Lives
under `src/plugins/bookmarks/` as a Cluster Q-style `.so` plugin.

**On-disk format** (`BookmarksStore.cpp:23-69`) — round-trips
`bookmarks.json` faithfully, matching addendum §2:

- Keys parsed: `type`, `ctime`, `path`, `subpath`, `title`, `query`,
  `options`, `items` (`BookmarksStore.cpp:9-18`). All other keys roll into
  `unknownKeys` (`:40-44`) and are written back verbatim
  (`BookmarksStore.cpp:65-67`). Cluster B unknown-key preservation idiom
  honoured.
- `ctime` is `qint64` ms-since-epoch (`BookmarkItem.h:22`). Matches.
- Recursive `items` for `group` type via `BookmarkItem::children`.
- Read at `onLoad` from `vault.readConfigJson("bookmarks.json")`
  (`BookmarksPlugin.cpp:43-46`), write debounced 500ms via
  `m_saveTimer` (`BookmarksPlugin.cpp:30-33, 75`). Addendum §6 says
  "~500 ms trailing" — match.

**Panel view** (`BookmarksView`, `:18-59`): `QTreeView` over a
`BookmarksModel`, customContextMenu via `customContextMenuRequested`. The
addendum-listed interactions (single-click open, middle-click new tab,
shift-click split, drag-reorder, right-click menu, `+` button) are partly
implemented:
- Click → `onActivated` (`BookmarksView.cpp:59`). Single-click opens; no
  evidence of middle-click or shift-click handling.
- Drag-reorder: not directly visible from the audit; `BookmarksStore::
  moveBookmark` (`:122-161`) exists as the backing API.
- Plus-button → `requestNewBookmark` signal (`:54`); plugin listens
  (`BookmarksPlugin.cpp:102-112`) and opens `BookmarkModal::runFor`.
- Right-click context menu (`BookmarksView.cpp:70`+) supports
  `removeBookmark` (`:145`).

**Commands** (`BookmarksPlugin.cpp:167-244`):
- `corbomite-bookmarks:open` — fully wired to reveal panel.
- `bookmarks:bookmark-current-file` — fully wired via
  `WorkspaceController::activeFilePath`. Uses `addCommandRaw` to preserve
  the `bookmarks:` prefix for `.obsidian/hotkeys.json` round-trip
  (`BookmarksPlugin.cpp:212`, comment §"Obsidian-id commands").
- `bookmarks:bookmark-all-tabs`, `bookmark-current-heading`,
  `bookmark-current-block`, `bookmark-current-search`,
  `bookmark-current-graph` — **registered as palette stubs**
  (`checkCallback returns false`, lines 222-243) because the underlying
  `WorkspaceController` lacks `openTabPaths`, `activeHeading`,
  `activeBlockId`, `activeSearchQuery`, `activeGraphOptions` accessors.
  Tracked as Cluster S follow-up. **Compatible reservation, behavioural gap.**

**Modal** (`BookmarkModal::runFor`, called at
`BookmarksPlugin.cpp:93, 111`): exists for the "Bookmark…" hamburger flow
and the panel `+` button.

**Lifecycle integration:** plugin listens to `VaultProxy::renamed`
(rewrites bookmark paths) and `VaultProxy::deletedFile` (marks orphaned via
`unknownKeys["_orphaned"] = true`) on lines 52-61. Addendum §6 doesn't
specify orphan handling; this is a Corbomite addition that's compatible
(the key rolls through unknown-key preservation).

**Concerns:**
- The 5 stubbed commands degrade vault interop — a hotkey defined for
  `bookmarks:bookmark-current-heading` will appear in the palette greyed,
  not function. Acceptable interim; would not satisfy a strict
  vault-portability test.
- Drag-reorder UX state (which groups expanded) is persisted in plugin
  `data.json` per addendum §3 last bullet; Corbomite stores it via
  `saveSessionState` (`BookmarksPlugin.cpp:116-144`), in
  `expanded[]` array of `/`-joined row paths. Functional equivalence;
  different on-disk shape.

## Canvas export-as-image

**Status: implemented.** `src/canvas/CanvasFileView.cpp:82-162` ships
the modal exactly as the addendum spec describes:
- Area radio (selected vs full, `selectedRadio` disabled when no selection,
  `:97-100`).
- Format combo PNG/SVG (`:106-108`).
- Transparent background and Show edges checkboxes (`:113-115`); show-edges
  defaults checked, transparent unchecked.
- Save dialog via `QFileDialog::getSaveFileName` (`:142-147`), default
  filename `canvas-export.<ext>`.
- PNG render via `scene->renderToImage(bounds, transparent, showEdges, 2.0)`
  at 2× density (`:151-152`) — addendum §3 specifies "2× density
  (Retina-friendly)". Match.
- SVG via `scene->renderToSvg(bounds, &file, transparent, showEdges)`
  (`:155-160`).
- Bounds computation: union of `selectedItems().sceneBoundingRect()` or
  `scene->itemsBoundingRect()` (`:128-135`). No 20px edge padding (addendum
  §3 step 2 says "Apply edge padding (20px)"). **Minor divergence.**
- Embedded-node rendering pipeline (markdown / image / PDF embeds rendered
  via their respective `export` paths per addendum §3 step 4): not separately
  audited; depends on what `CanvasScene::renderToImage` does internally.
- Hamburger menu (`onMoreOptionsMenu`, `:164-221`) wires "Split right",
  "Split down", a disabled "Bookmark…" placeholder, "Export as image", and
  the `view.linked → Open backlinks` submenu. Matches addendum §1 (Canvas
  hamburger) and the Cluster R design spec referenced.

The `canvas:export-as-image` command-id from the addendum (§1) is **not
visible** as a registered Command — the export is reached only through the
hamburger menu action. A user with a hotkey bound to `canvas:export-as-image`
in `.obsidian/hotkeys.json` would be unable to fire the export. Filing as
minor concern.

Other Canvas commands from addendum §1 (`canvas:new-file`,
`canvas:jump-to-group`, `canvas:convert-to-file`) are **not implemented**;
the Canvas plugin shell is more skeletal than the addendum.

## Graph screenshot

**Status: implemented.** `src/plugins/graph-view/GraphView.cpp:91-98`
adds the "Copy screenshot" hamburger entry; it dispatches the
`corbomite-graph-view:copy-screenshot` command, registered in
`GraphViewPlugin.cpp:60-83`:

```
const QImage img = target->grab().toImage();
if (!img.isNull())
    QApplication::clipboard()->setImage(img);
```

Matches the addendum §4 implementation hint exactly: `QWidget::grab` is
synchronous and respects DPR, no separate `QGraphicsScene::render`
needed. Falls back to grabbing the GraphView itself if the inner
`graphWidget()` is null (`:74-75`).

**Divergences:**
- No "Could not copy screenshot" Notice on null grab. The early-return on
  `img.isNull()` is silent (`GraphViewPlugin.cpp:77-79`). Addendum §4
  says "show 'Could not copy screenshot — graph not ready.' Notice and
  bail." Minor.
- No "Screenshot copied to clipboard." success Notice either (addendum §2
  step 4). Equally minor.
- The command id is `corbomite-graph-view:copy-screenshot` — the addendum's
  Obsidian id is `graph:copy-screenshot`. Round-tripping a hotkeys.json that
  binds `graph:copy-screenshot` would fail to fire the screenshot.
  Compatible **reservation** would be `addCommandRaw` with `graph:` prefix
  (the bookmarks plugin does this for its 6 commands; graph-view does not).
  Filing as minor concern.

The "Copy screenshot" menu entry uses `corbomite-graph-view:copy-screenshot`
internally to match the registered command id; consistent within the
plugin, divergent from Obsidian.

## Daily Notes + Templates schemas

**Status: partially implemented.** `Corbomite::DailyNoteService` and
`Corbomite::TemplateService` (`libs/models/`) read the Obsidian config
files via `Corbomite::VaultConfig` (`libs/storage/`), which exposes
`readDailyNotesJson()` and (per addendum) corresponding template
accessors. Confirmed at `DailyNoteService.cpp:43-63`:

```
const auto json = config.readDailyNotesJson();
if (json) {
    obj.value("format") → setDateFormat
    obj.value("folder") → setFolder
    obj.value("template") → setTemplateName
}
```

Three out of four addendum schema keys consumed. The fourth, `autorun`,
is **explicitly noted as ignored**: `DailyNoteService.cpp:60-62` comments
"The `autorun` key is documented but not consumed here; MainWindow can
read it separately if boot-time auto-open is wanted." So a vault with
`autorun: true` will not auto-open today's daily note in Corbomite. Minor.

`TemplateService::initFromVaultConfig` exists (`TemplateService.cpp:147`).
The audit didn't follow each key but the public surface
(`TemplateService.h:48`) supports `folder`, `date_format`, `time_format`
which are the three keys in the templates addendum.

**Round-trip preservation of unknown keys**: not directly audited per
service; depends on `VaultConfig::mergeJson` discipline. The
`MainWindow.cpp:2448` site uses `vc.mergeJson("appearance.json", upd)`
which suggests the merge-don't-overwrite pattern is in place.

## --- Top concerns ---

## Notable concerns / suspected bugs

1. **`resolveSubpath` block-id case sensitivity** (`LinkUtils.cpp:121-155`).
   Obsidian lowercases the lookup; Corbomite scans `source` for the literal
   marker. `[[Note#^MyBlock]]` against `^myblock` definition resolves in
   Obsidian and silently fails in Corbomite. **Vault-format compat bug.**

2. **`resolveSubpath` multi-segment heading walk not implemented**
   (`LinkUtils.cpp:170-189` matches only the trailing segment). Self-noted
   in the comment; vault-portability divergence for heading-path subpaths.

3. **Dangling adapter pointers in `onVaultClosed`** (`MainWindow.cpp:2190-
   2263`). `m_linkResolverAdapter` / `m_metadataCacheAdapter` /
   `m_metadataParserImpl` close over `m_linkResolver` / `m_metadataCache`
   raw pointers; the adapters are *not* `.reset()`-ed before the underlying
   services are deleted. A short-lived window of use-after-free risk if any
   consumer (the embed renderer is set to nullptr; nothing else inspected)
   still holds a copy.

4. **`css-change` and `quit` events not emitted.** `MainWindow` has no
   bridge from `m_themeService::themeChanged` onto a workspace-level
   `css-change`-equivalent event. `closeEvent` runs `confirmCloseUnsaved` +
   `saveSessionState` directly without firing a `quit`-equivalent that
   would let plugins schedule async cleanup futures (no `Eb` collector).
   Both are plugin-API gaps; both block Obsidian-faithful plugin authoring.

5. **Embed registry leaks plugin factories across vault switches.**
   `m_embedRegistry` is built once at MainWindow construction; plugin embed
   factories registered at `onLoad` survive vault switches. If they captured
   per-vault state (`m_vaultObj`, `m_metadataCache`), they dangle.
   Currently latent (no plugin registers embeds); becomes a real bug as soon
   as one does.

6. **No actual `Scope` users in the tree.** `ScopeManager::pushScope` is
   called from zero call sites outside the manager itself. Modal /
   suggester containment via `Scope` doesn't happen — keys flow through
   normal Qt shortcut routing. The infrastructure exists; nothing uses it.

7. **No `appId` equivalent.** Per-vault process namespace is missing.
   QtKeychain entries, IndexedDB-equivalent caches, and webview partitions
   (when added) cannot be vault-scoped without one. Section "Per-vault
   isolation risk" is downstream of this.

8. **No `dragManager`.** Plugins cannot coordinate drag MIME types or
   observe global drag state. Surfaces matter only when plugins try.

9. **Bookmarks: 5 of 7 commands are palette stubs.** `bookmarks:bookmark-
   current-heading`, `-current-block`, `-current-search`, `-current-graph`,
   `bookmark-all-tabs` are registered with `checkCallback → false` because
   `WorkspaceController` lacks the accessors. Tracked as Cluster S
   follow-up; behavioural gap for vault-shared hotkeys.

10. **Graph screenshot and Canvas export use Corbomite-prefixed command
    ids.** `corbomite-graph-view:copy-screenshot` instead of
    `graph:copy-screenshot`; no command id at all for canvas export. Vault
    `hotkeys.json` round-trip would not fire these. Bookmarks plugin
    handles this correctly via `addCommandRaw`; the other two should
    follow the pattern.

11. **MomentFormatter missing tokens:** `Y`, `Q`, `gg`/`gggg`, `e`/`E`,
    `k`/`kk`, `Z`/`ZZ`, locale shortcuts (`L`, `LL`, `LT`, etc.). Vault
    templates using these render literal characters. Mechanical follow-up.

12. **No Platform feature-flag accessors** (`isDesktop`/`canExportPdf`/
    `canPopoutWindow`/`canStackTabs`/`supportsIndexedDb`). Plugins cannot
    negotiate host capabilities.

13. **SecretStorage no `isAvailable()`**, no vault scoping (downstream of
    missing `appId`). Same plugin in two vaults shares secrets.

14. **DailyNoteService ignores `autorun`** (`DailyNoteService.cpp:60-62`).
    Documented gap; mechanical to wire from MainWindow.

15. **File Recovery is per-file `.md` files in `.obsidian/file-recovery/`**
    (`TextFileView.cpp:140`), not Obsidian's single-file
    `.obsidian/file-recovery.json` with `{maxSnapshotAge,
    maxSnapshotsPerFile, entries}`. No periodic snapshots, no Version
    History modal, no restore. Cluster T not scheduled.
