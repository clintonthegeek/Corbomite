# PLUGIN-API-SKETCH — Extension-point surface for a future Corbomite plugin API

This document is a **design target**, not an implementation plan. It specifies the extension-point shape Corbomite should mirror so that, when the plugin API ships, Obsidian plugins can be ported with minimal diff and Corbomite's own internal features can slot into the same registration paths.

The primary inputs are `plugin.md` (full verb enumeration) and `02-extension-surfaces.md` (cross-domain census). Workspace-owned registries are cross-referenced from `workspace.md §7`. Component lifecycle comes from `ui-bundle.md §1 Component`. View hierarchy from `views.md`. Core primitives (`App`, `Events`, `Scope`) from `core.md`.

## 1. Scope & non-goals

**In scope.** Spec the shape of Corbomite's eventual plugin API — registration verbs, lifecycle primitives, event and registry contracts, subclass entry points — so that building foundational systems now (Component, Events bridge, ViewRegistry, MenuSectionRouter, …) doesn't require refactoring when plugin support lands.

**Not in scope today.**
- Actual plugin-API implementation.
- Plugin-store UI or auto-update.
- Cross-compatible C++ plugin ABI (Corbomite should ship JavaScript plugins in a V8/WebEngine sandbox, reusing the Obsidian API verbatim, rather than inventing a C++ ABI that will never achieve parity).

**Compat posture.** The goal is **port-with-minimal-diff** for the top ~80% of Obsidian plugins. Plugins that depend on CodeMirror internals, Electron IPC, Node APIs, or Chromium DOM APIs outside the public plugin surface will not port — this is explicit and documented in §12. Plugins that use only the documented `obsidian` module exports should port cleanly.

**Timing.** Plugin API is a **post-1.0 target**. The reason to spec now is that retrofitting the lifecycle primitives (Component, Events, Scope, ViewRegistry) after-the-fact is expensive. We build the primitives now; plugin loading turns on later.

## 2. Plugin lifecycle

### Manifest

Each plugin ships a `manifest.json` under `<vault>/.obsidian/plugins/<id>/` (plugin.md §2; VAULT-FORMAT.md §3 "manifest.json"). Fields:

```typescript
{
  id: string;               // unique; used as command prefix + ribbon id
  name: string;             // human-readable
  version: string;          // semver
  minAppVersion?: string;   // required minimum Obsidian API version
  description?: string;
  author?: string;
  authorUrl?: string;
  fundingUrl?: string | Record<string, string>;
  isDesktopOnly?: boolean;
  // `dir` injected by loader at load time
}
```

**Corbomite note.** A plugin compiled against Corbomite's shim should expose an `apiVersion` string that Obsidian-style `requireApiVersion` can gate against. Corbomite reserves a distinct version namespace from Obsidian proper; a plugin targeting both would query `app.isCorbomite` or similar.

### Load / unload lifecycle

`Plugin extends Component`. The lifecycle is (plugin.md §1, §8):

1. **Construction.** `new Plugin(app, manifest)` — injected manifest carries `dir`. Boot-minimal constructor only; no expensive work yet.
2. **`load()`.** Calls `onload()` (the plugin's override), then loads children. If `onload()` throws, the promise rejects but registered cleanups remain in `_events` — **known gap** (plugin.md §8 "Throws from `onload`"). Corbomite should improve: on throw, auto-`unload()`.
3. **`unload()`.** Drains `_children` LIFO, then `_events` cleanup thunks LIFO, then calls `onunload()`. Idempotent via `_loaded` guard.
4. **`enable` (user) vs `load` (boot).** Boot auto-loads plugins listed in `community-plugins.json`. User enable/disable sets `_userDisabled = true` before calling `unload()`. **Key difference:** on user-disable, Obsidian additionally calls `workspace.detachLeavesOfType(type)` for every registered view-type — so open tabs of a disabled plugin close, not just at next restart (plugin.md §10 `registerView`).
5. **`onUserEnable()`.** Empty default. Called once per session after the user first toggles on — typically used for welcome notices.
6. **`onExternalSettingsChange()`.** Optional. Invoked when `data.json` mtime advances past `_lastDataModifiedTime` (50 ms debounce). See §7.

**Hot-reload distinction.** Obsidian does not hot-reload. To apply a plugin update, the user must toggle off-then-on via settings. Corbomite should match.

### Corbomite equivalent

```cpp
// libs/pluginhost/Plugin.h (sketch)
class Plugin : public Component {
public:
    Plugin(App* app, const PluginManifest& manifest);
    virtual void onLoad() {}
    virtual void onUnload() {}
    virtual void onUserEnable() {}
    virtual void onExternalSettingsChange() {}

    const PluginManifest& manifest() const;
    App* app() const;

    // All registration verbs below inherit from Component's cleanup mechanism.
    QAction* addCommand(const CommandSpec&);
    RibbonButton* addRibbonIcon(QString icon, QString title, RibbonCallback);
    QWidget* addStatusBarItem();
    void addSettingTab(PluginSettingTab*);
    void registerView(QString type, ViewFactory);
    void registerExtensions(QStringList exts, QString type);
    void registerHoverLinkSource(QString id, HoverLinkSource);
    void registerObsidianProtocolHandler(QString action, ProtocolHandler);
    void registerEditorSuggest(EditorSuggest*);
    void registerMarkdownPostProcessor(MarkdownPostProcessor, int sortOrder);
    void registerMarkdownCodeBlockProcessor(QString lang, CodeBlockProcessor, int sortOrder);

    // Data persistence
    QFuture<QJsonDocument> loadData();
    QFuture<void> saveData(const QJsonDocument&);
};
```

## 3. Component lifecycle

`Component` is the universal base class — every long-lived plugin object (Plugin, View, Modal, MarkdownRenderChild, HoverPopover, Menu) extends it (ui-bundle.md §1).

### Obsidian contract

```typescript
class Component {
  _loaded: boolean;
  _children: Component[];
  _events: Array<() => void>;

  load(): void;                       // no-op if already loaded
  unload(): void;                     // drains children LIFO, then _events LIFO, then onunload
  onload(): void;                     // override point
  onunload(): void;                   // override point
  addChild(child: Component): Component;        // auto-loads child if parent loaded
  removeChild(child: Component): Component;     // auto-unloads
  register(cb: () => void): void;               // generic cleanup thunk
  registerEvent(ref: EventRef): void;           // auto-offref on unload
  registerDomEvent(el, type, cb, opts?): void;  // auto-removeListener on unload
  registerInterval(handle: number): number;      // auto-clearInterval on unload
  registerScopeEvent(kmRef): void;              // auto-unregister on unload
}
```

**Invariants** (ui-bundle.md §8):
- Cleanup is **LIFO**. Last registered runs first.
- `load()` and `unload()` are idempotent behind `_loaded`.
- `addChild(c)` on an already-loaded parent auto-loads the child; no second `onload` when the parent reloads.

### Corbomite translation

```cpp
// libs/core/Component.h (sketch)
class Component : public QObject {
    Q_OBJECT
public:
    explicit Component(QObject* parent = nullptr);

    void load();
    void unload();
    virtual void onLoad() {}
    virtual void onUnload() {}

    void addChild(Component* child);
    void removeChild(Component* child);
    void registerCleanup(std::function<void()>);
    void registerConnection(QMetaObject::Connection);   // auto-disconnect on unload
    void registerDomEvent(/* not applicable — use signals */);
    void registerInterval(QTimer*);                      // auto-stop-and-delete on unload

    bool isLoaded() const { return m_loaded; }

private:
    bool m_loaded = false;
    QList<Component*> m_children;
    QList<std::function<void()>> m_cleanups;  // LIFO
};
```

Qt's `QObject` parent-child relationship handles `QObject` destruction; `Component` adds the generic cleanup-thunk queue that Qt lacks. The two mechanisms compose: a `Component` parent destroys child `QObject`s via parent-pointer and invokes registered cleanup-thunks via `unload()`.

## 4. Event mixin / Qt-signal bridge

### Obsidian contract

The `Events` class is mixed into Vault, Workspace, MetadataCache, ViewRegistry, WorkspaceItem, WorkspaceLeaf, and any plugin-defined emitter (core.md §1).

```typescript
class Events {
  _: Record<string, EventRef[]>;

  on(name: string, fn: Function, ctx?: any): EventRef;
  off(name: string, fn: Function): void;      // identity-match on fn; removes ALL matching
  offref(ref: EventRef): void;                // scalpel
  trigger(name: string, ...args: any[]): void;
  tryTrigger(ref: EventRef, args: any[]): void;  // try/catch + setTimeout(throw, 0)
}
```

**Semantics** (core.md §1, §8):
- `trigger` iterates a `slice()` snapshot, so a listener can self-`off` without shifting subsequent listeners.
- `tryTrigger` swallows exceptions synchronously and re-throws them via `setTimeout(0)` — one listener throwing never aborts dispatch.
- Multiple listeners for the same `(name, fn)` collide on `off`; plugins should always use `offref(ref)` or `Component.registerEvent(ref)`.

### Corbomite translation

The challenge: Qt signals are statically typed (`void something(QString)`), while Obsidian events are dynamic (name + arbitrary args).

**Recommended approach — hybrid:**

1. **Qt-native signals for Corbomite-internal code.** `Vault::fileCreated(NoteMeta*)`, `Vault::fileModified(NoteMeta*)`, etc. Full static type-safety.
2. **`Events` mixin facade for plugin-facing compat.** An `Events` mixin that stores `(name → QList<EventRef>)` and dispatches via `QVariantList`. Plugins call `vault->on("create", [](QVariant arg) {...})` and get the same behaviour as Obsidian. The mixin wraps the underlying Qt signals: `Vault::fileCreated` connects to the mixin to fan-out to `"create"` listeners.
3. **Async re-throw via `QTimer::singleShot(0, [] { throw; })`** inside a per-listener `try/catch`.

```cpp
// libs/core/Events.h (sketch)
class Events {
public:
    struct EventRef {
        Events* e;
        QString name;
        std::function<void(const QVariantList&)> fn;
    };

    EventRef* on(const QString& name, std::function<void(const QVariantList&)> fn);
    void off(const QString& name, std::function<void(const QVariantList&)> fn);
    void offref(EventRef* ref);
    void trigger(const QString& name, const QVariantList& args);
    void tryTrigger(EventRef* ref, const QVariantList& args);

private:
    QHash<QString, QList<EventRef*>> m_listeners;
};

// Subclasses multiply-inherit from QObject + Events
class Vault : public QObject, public Events { ... };
```

**Subscription reference lifecycle.** `EventRef*` returned by `on()` is owned by the emitter's list. `offref(ref)` removes from the list and deletes. `Component::registerEvent(ref)` stores the ref and `offref`-s on unload.

**Events Corbomite must expose** (complete list from Pass 2):

| Emitter | Event names | Source domain |
|---|---|---|
| `Vault` | `create`, `modify`, `delete`, `rename`, `closed`, `raw`, `config-changed` | `vault.md §4` |
| `MetadataCache` | `changed`, `deleted`, `resolve`, `resolved`, `finished` | `metadata.md §4` |
| `Workspace` | `layout-ready`, `layout-change`, `active-leaf-change`, `file-open`, `quick-preview`, `resize`, `window-frame-change`, `swipe`, `file-menu`, `files-menu`, `url-menu`, `editor-menu`, `markdown-viewport-menu`, `leaf-menu`, `tab-group-menu`, `hover-link`, `window-open`, `window-close`, `css-change`, `quit`, `post-processor-change`, `markdown-scroll` | `workspace.md §4` |
| `ViewRegistry` | `view-registered`, `view-unregistered`, `extensions-updated` | `views.md §4` |
| `WorkspaceLeaf` | `pinned-change`, `group-change`, `history-change` | `workspace.md §4` |

**Pass 1 correction (important).** `App.prototype.on` is a **no-op**. `app.on('css-change')` silently registers nothing. Workspace-level events fire on `app.workspace` (core.md §1, §8). Corbomite must NOT expose a plausible-but-inert `app.on` — either make `app.on` delegate to `app.workspace.on`, or omit the method entirely and throw `NotImplementedError`.

## 5. Registries

Every plugin extension point registers into one of these registries. For each: canonical Obsidian API, typical plugin use case, Corbomite-idiomatic translation.

### 5.1 ViewRegistry

- **Purpose.** Map `viewType` → factory; `extension` → `viewType`.
- **Owner.** `app.viewRegistry` (views.md §1).
- **Registration verb.** `Plugin.registerView(type, factory)` + `Plugin.registerExtensions(exts, type)`.
- **Plugin use case.** Add a new file-type view (`.kanban`, `.excalidraw`, `.drawio.svg`, `.odt`); add a sidebar panel (outline, calendar, review).
- **Obsidian API.** `viewRegistry.registerView(type, factory)` throws on dup; `registerExtensions(exts, type)` atomic across array.
- **Corbomite API.**
  ```cpp
  // libs/workspace/ViewRegistry.h
  class ViewRegistry : public QObject, public Events {
      Q_OBJECT
  public:
      using Factory = std::function<View*(WorkspaceLeaf*)>;
      void registerView(const QString& type, Factory);          // throws on dup
      void unregisterView(const QString& type);
      void registerExtensions(const QStringList& exts, const QString& type);  // atomic
      void unregisterExtensions(const QStringList& exts);
      Factory getFactory(const QString& type) const;
      QString typeByExtension(const QString& ext) const;
      bool isExtensionRegistered(const QString& ext) const;
  signals:
      void viewRegistered(QString type);
      void viewUnregistered(QString type);
      void extensionsUpdated();
  };
  ```

### 5.2 EmbedRegistry

- **Purpose.** Map extension → embed factory for `![[file]]` inline embeds.
- **Owner.** `app.embedRegistry` (core.md §2 `aJ`).
- **Registration verb.** Not on `Plugin` directly — Bases-plugin / Canvas-plugin extend via `linkUpdaters` or direct assignment (metadata.md §7). Plugin API: TBD.
- **Plugin use case.** Register a custom embed type (e.g. `.drawio` renders as draw.io diagram inline).
- **Corbomite API.**
  ```cpp
  class EmbedRegistry : public QObject {
  public:
      using Factory = std::function<Embed*(const EmbedContext&)>;
      void registerEmbed(const QString& ext, Factory);
      Factory factoryFor(const QString& ext) const;
  };
  ```

### 5.3 EditorSuggest registry

- **Purpose.** Inline autocomplete suggesters triggered by typed patterns.
- **Owner.** `app.workspace.editorSuggest` (an `EditorSuggestManager` — views/ViewRegistry.js:238).
- **Registration verb.** `Plugin.registerEditorSuggest(suggest)`.
- **Plugin use case.** `@mention` completion, custom `[[link]]` completion, slash-command popup.
- **Iteration semantics.** Insertion-order; first non-null `onTrigger` wins. Built-ins registered first → plugin overrides of `[[`/`#` are shadowed, not prioritised (editor.md §12). **Compat-critical** — plugins depend on this behaviour.
- **Corbomite API.**
  ```cpp
  class SuggestRegistry {
  public:
      void addSuggest(EditorSuggest* s);    // appends; insertion order
      void removeSuggest(EditorSuggest* s);
      // Internal: called per keystroke; iterates in insertion order
      bool tryTrigger(const TriggerContext&);
  };
  ```

### 5.4 HoverLinkSource registry

- **Purpose.** Declare where hover-previews originate; Page-Preview plugin uses to decide whether to open a popover based on modifier-key.
- **Owner.** `app.workspace.hoverLinkSources` (workspace.md §7).
- **Registration verb.** `Plugin.registerHoverLinkSource(id, {display, defaultMod})`.
- **Plugin use case.** A custom view type wants hover previews to work for its link spans.
- **Built-ins.** `search`, `preview`, `editor`, `tab-header`. **Bases hardcodes `source: "bases"`** (rendering.md §1).
- **Corbomite API.**
  ```cpp
  struct HoverLinkSource { QString display; bool defaultMod; };
  class Workspace {
  public:
      void registerHoverLinkSource(const QString& id, HoverLinkSource);
      void unregisterHoverLinkSource(const QString& id);
      HoverLinkSource sourceFor(const QString& id) const;
  };
  ```

### 5.5 ObsidianProtocol handler registry

- **Purpose.** Handle `obsidian://<action>?...` URLs.
- **Owner.** `app.workspace.protocolHandlers` (workspace.md §7).
- **Registration verb.** `Plugin.registerObsidianProtocolHandler(action, handler)`. **Throws on duplicate** — plugins must `unregister` first.
- **Built-ins** (workspace.md §7): `open`, `new`, `search`, `show-plugin`, `show-theme`, `show-release-notes`, `debug-info`, `publish-sites`, `sync-setup`, `vault-setup`, `hook-get-address`.
- **Corbomite API.**
  ```cpp
  using ProtocolHandler = std::function<void(const QMap<QString, QString>& args)>;
  class Workspace {
  public:
      void registerObsidianProtocolHandler(const QString& action, ProtocolHandler);
      void unregisterObsidianProtocolHandler(const QString& action);
  };
  ```
- Implemented via `KDBusService(Unique)` (single-instance) + custom URL scheme handler. Register both `obsidian://` and `corbomite://`.

### 5.6 EditorExtension registry

- **Purpose.** Plugin-supplied CodeMirror 6 extensions (StateField, ViewPlugin, Decoration).
- **Owner.** `app.workspace.editorExtensions` (workspace.md §7).
- **Registration verb.** `Plugin.registerEditorExtension(ext)`.
- **Semantics.** Flat list applied to **every** `MarkdownView` via `updateOptions()` → CM `Compartment` reconfigure (editor.md §12). No per-leaf scoping.
- **Corbomite note.** Markoff is not CodeMirror. A literal port is impossible. Corbomite should expose a different extension model: `Markoff::EditorPlugin` with virtual hooks (`onKeyPress`, `onSelectionChange`, `customPainter`, `decorateRange`). Plugin authors shipping a CM6 extension will not port — document explicitly.

### 5.7 MarkdownPostProcessor registry

- **Purpose.** DOM walker run after markdown render; plugins mutate rendered output.
- **Owner.** `MarkdownPreviewRenderer.postProcessors` (editor-markdown.md §7, static).
- **Registration verb.** `Plugin.registerMarkdownPostProcessor(fn, sortOrder)`.
- **Firing.** Post-processors run in `sortOrder` ascending; promise returns awaited in `ctx.promises`. Every register/unregister fires `post-processor-change` on workspace so live `MarkdownPreviewView`s re-render (plugin.md §10).
- **Plugin use case.** Dataview, Tasks, natural-language-dates, tag pills, math-inline processors.
- **Corbomite API.**
  ```cpp
  using MarkdownPostProcessor = std::function<QFuture<void>(QWidget* sectionEl, MarkdownPostProcessorContext&)>;
  class MarkoffRenderEngine {
  public:
      void registerPostProcessor(MarkdownPostProcessor, int sortOrder = 0);
      void unregisterPostProcessor(MarkdownPostProcessor);
  signals:
      void postProcessorChange();
  };
  ```

### 5.8 MarkdownCodeBlockProcessor registry

- **Purpose.** Claim a fenced-code language.
- **Owner.** `MarkdownPreviewRenderer.codeBlockPostProcessors` (editor-markdown.md §7).
- **Registration verb.** `Plugin.registerMarkdownCodeBlockProcessor(lang, fn, sortOrder)`. **Throws on duplicate `lang`** (plugin.md §10, editor-markdown.md §7). Plugin wrapper generates a post-processor wrapping `fn` with a `div.block-language-<lang>` shell and a `ctx.replaceCode(newSrc)` back-channel.
- **Plugin use case.** ```` ```dataview ````, ```` ```tracker ````, ```` ```chart ```` code blocks.
- **Corbomite API.**
  ```cpp
  using CodeBlockProcessor = std::function<QFuture<void>(const QString& source, QWidget* el, CodeBlockContext&)>;
  class MarkoffRenderEngine {
  public:
      void registerCodeBlockProcessor(const QString& lang, CodeBlockProcessor, int sortOrder = 0);
      void unregisterCodeBlockProcessor(const QString& lang);
  };
  ```
- **Compat blocker.** `ctx.replaceCode(newSrc)` requires source-position tracking in the scene graph. Not currently in Markoff.

### 5.9 Command registry

- **Purpose.** Named commands surfaced in the palette + hotkey-bindable.
- **Owner.** `app.commands` (`Y6`; core.md §7).
- **Registration verb.** `Plugin.addCommand(cmd)`. **Mutates `cmd.id` and `cmd.name` in place** — prefixes with `<manifest.id>:` and `<manifest.name>: ` (plugin.md §8). Returns the mutated spec.
- **Plugin use case.** Every plugin that adds user-accessible functionality.
- **`CommandSpec`:**
  ```typescript
  {
    id: string;
    name: string;
    icon?: string;
    hotkeys?: Hotkey[];
    mobileOnly?: boolean;
    repeatable?: boolean;
    // exactly one of:
    callback?: () => void;
    checkCallback?: (checking: boolean) => boolean | void;
    editorCallback?: (editor: Editor, view: MarkdownView) => void;
    editorCheckCallback?: (checking: boolean, editor, view) => boolean | void;
  }
  ```
- **Dispatch rules** (core.md §7):
  - `callback` — always runnable.
  - `checkCallback(true)` — availability check; `checkCallback(false)` runs.
  - `editorCallback` / `editorCheckCallback` — only dispatch when `app.workspace.activeEditor` is an Editor.
- **Corbomite API.** `CommandRegistry` wrapping `KActionCollection`. `checkCallback` → `QAction::isEnabled()`. Namespaced IDs for `.obsidian/hotkeys.json` compat.

### 5.10 SettingsTab registry

- **Purpose.** Add a tab to the Settings modal.
- **Owner.** `app.setting` (settings.md §1).
- **Registration verb.** `Plugin.addSettingTab(tab)`. `PluginSettingTab` subclass with `display(): void` override. `containerEl` is live; plugin must call `containerEl.empty()` first in every `display()` call.
- **Sorting.** Core tabs in registration order; plugin tabs A-Z by name.
- **Corbomite API.**
  ```cpp
  class PluginSettingTab : public QWidget {
  public:
      PluginSettingTab(App*, Plugin*);
      virtual void display() = 0;         // rebuild UI
      virtual void hide() {}
      QString id() const;                  // usually plugin.manifest.id
      QString name() const;
  };
  class SettingsDialog : public KPageDialog {
  public:
      void addPluginPage(PluginSettingTab*);
      void removePluginPage(PluginSettingTab*);
  };
  ```

### 5.11 RibbonIcon registry

- **Purpose.** Left-edge activity-bar button.
- **Owner.** `WorkspaceRibbon.items` (workspace.md §7).
- **Registration verb.** `Plugin.addRibbonIcon(icon, title, cb)`. Registers id as `"<manifest.id>:<title>"` — two calls with same title collide (plugin.md §10).
- **Persistence.** `['left-ribbon'].hiddenItems` in workspace.json, key order = runtime item order.
- **Corbomite API.** `RibbonBar::addButton(id, icon, title, cb)`. Drag-reorder via `Gc`-equivalent. Right-click hides individual items.

### 5.12 StatusBar registry

- **Purpose.** Bottom status-bar item.
- **Owner.** `app.statusBar` (`Nee`).
- **Registration verb.** `Plugin.addStatusBarItem()` — returns an `HTMLElement` pre-classed `plugin-<sanitised-id>`. **Sanitiser bug:** regex without `g` flag, only first illegal char rewritten (plugin.md §2). Preserve for compat.
- **Corbomite API.** `QStatusBar::addPermanentWidget(new QWidget)` with `objectName` set to sanitised id for QSS styling.

### 5.13 Menu section registry

- **Purpose.** Named sections within a `Menu`; plugins hook `file-menu`/`url-menu`/etc. events to inject items into specific sections.
- **Owner.** Per-`Menu` instance (ui-bundle.md §2 "section registry").
- **Canonical sections** (workspace.md §10, ui-bundle.md §2):
  - File-menu: `["title", "open", "action-primary", "action", "info", "info.copy", "view", "system", "", "danger"]`.
  - Tab-header menu: `["title", "close", "pane", "open", "action", "find", "info", "info.copy", "view", "view.linked", "system", "", "danger"]`.
  - Tab-group menu: `["action", "close", "", "tablist"]`.
  - Editor-menu (inferred): `["selection", "action", "open", "view", "info", "system"]`.
  - `""` = uncategorised bucket; items without `setSection` land here.
- **Corbomite API.**
  ```cpp
  class MenuSectionRouter {
  public:
      void declareSections(const QStringList& ids);
      void setSectionSubmenu(const QString& id, const QString& title, const QString& icon = {});
      QAction* addItem(const QString& section, QAction*);
      void applyTo(QMenu*) const;   // sort, insert separators, wrap submenus
  };
  ```

### 5.14 Icon registry

- **Purpose.** Inject custom SVG icons.
- **Owner.** `ui/icons/addIcon.js` → `Xm` (ui-bundle.md §7).
- **Registration verb.** `addIcon(name, svgInnerHtml)`. `removeIcon(name)` removes. Lucide `lucide-*` names reserved (re-registering loses to built-in). **No auto-cleanup on plugin unload** — plugins re-register in `onload`.
- **Corbomite API.**
  ```cpp
  class IconRegistry {
  public:
      void registerCustom(const QString& name, const QString& svg);
      void removeCustom(const QString& name);
      QIcon getIcon(const QString& name) const;   // resolution: lucide → custom → legacy → alias
      QStringList ids() const;
  };
  ```
- **Icon translation table** (ui-bundle.md §11): ~125 Lucide IDs → Freedesktop theme names (~70% direct match) + bundled SVG fallback for the rest.

### 5.15 OperatorFuncConfigs (Bases)

- **Purpose.** Plugin-added filter/formula operators for Bases.
- **Owner.** `app.workspace.operatorFuncConfigs` (workspace.md §7; bases.md §10).
- **Registration verb.** `workspace.registerOperatorFuncConfigs(id, config)`.
- **Corbomite API.** Part of `libs/formula/` parser (Bases cluster, GAP-ANALYSIS §Cluster K).

## 6. Subclass entry points

### 6.1 View hierarchy

```
Component
└── View                  (abstract; getViewType, getDisplayText, getIcon, onOpen/onClose)
    └── ItemView          (adds headerEl + actionsEl + onMoreOptionsMenu)
        └── FileView      (adds file: TFile | null; onLoadFile/onUnloadFile)
            └── EditableFileView   (inline title rename)
                └── TextFileView   (getViewData/setViewData/clear; 2 s debounced save; 3-way merge)
                    └── MarkdownView  (built-in for .md)
                    └── BasesView    (built-in for .base)
                    └── (plugin subclasses — .kanban, .excalidraw, …)
```

(views.md §1.)

**Required overrides per class:**

- `View`: `getViewType(): string`, `getDisplayText(): string`, `getIcon(): string`, `onOpen/onClose: Promise<void>`.
- `ItemView`: + `onMoreOptionsMenu(menu)` for `"…"` button.
- `FileView`: + `onLoadFile(file)`/`onUnloadFile(file)`.
- `TextFileView`: + `getViewData(): string`, `setViewData(data, clear): void`, `clear(): void`.

**Lifecycle contract** (views.md §8): `load()` runs once per instance, between `open()` and `close()`. `onload` fires before `onOpen`; `onOpen` runs with `containerEl` in the DOM.

**Typical plugin pattern:** Subclass `ItemView` for a sidebar panel; subclass `TextFileView` for a new file type.

### 6.2 Modal / SuggestModal

```
Modal (owns own Scope; no Component mixin)
├── SuggestModal<T>      (abstract; getSuggestions/renderSuggestion/onChooseSuggestion)
│   └── FuzzySuggestModal<T>   (getItems/getItemText/onChooseItem with built-in fuzzy)
├── (nb/ib) confirm/prompt    (internal; plugins use KMessageBox-equivalent)
```

(ui-bundle.md §1.)

**Typical plugin pattern:** subclass `FuzzySuggestModal` for "pick from finite list" pickers (template picker, prompt-for-link).

**Selection restore on close.** `Modal.shouldRestoreSelection = true` snapshots and restores both focus widget AND `QTextCursor` range (ui-bundle.md §8). Corbomite's `QDialog` restores focus widget only; extend `closeEvent` for cursor-range restore.

### 6.3 AbstractInputSuggest

Popover suggester anchored to a form input (as opposed to a full modal):

```
PopoverSuggest
└── AbstractInputSuggest<T>   (getItems/renderSuggestion/selectSuggestion)
    ├── xI (file-path)         — internal
    ├── TI (md-only)            — internal
    ├── AI (folder)             — internal
    └── (plugin subclasses)
```

**Use case.** Autocomplete inside a settings-form text field (e.g. "default template" input with live file-path completion).

### 6.4 EditorSuggest

Inline autocomplete popup triggered by typing patterns in the editor.

**Required overrides** (editor.md §1):
- `onTrigger(cursor, editor, file): TriggerInfo | null` — decide whether to open.
- `getSuggestions(ctx): T[] | Promise<T[]>`.
- `renderSuggestion(item, el)`.
- `selectSuggestion(item, evt)`.

**Lifecycle.** Manager iterates suggesters in insertion order; first non-null `onTrigger` wins. Built-ins (`[[`, `#`) registered first — plugin overrides **shadow**, not prioritise (editor.md §12).

**`context` replaced, not merged.** `EditorSuggest.context` is overwritten on every trigger, not merged. Plugin authors expecting persistence across keystrokes are surprised (editor.md §12).

**Async `getSuggestions` re-checks `editor.hasFocus()` before showing** (editor.md §12). Corbomite must mirror.

### 6.5 PluginSettingTab

See §5.10. `display()` is the only required override; rebuilds UI on every tab navigation.

### 6.6 MarkdownRenderChild

```
Component
└── MarkdownRenderChild   (returned from a post-processor via ctx.addChild(child))
```

**Purpose.** Lifecycle wrapper for plugin DOM fragments. When `MarkdownPreviewSection` is recycled, every registered child's `unload()` fires — plugins detach listeners + cancel work + free resources (editor-markdown.md §1; rendering.md §10).

**Typical plugin pattern:** inside a code-block processor, return `new MyChild(el)` so DOM cleanup is automatic.

### 6.7 Plugin

The root class (§2). Subclasses override `onload`, `onunload`, optionally `onUserEnable`, `onExternalSettingsChange`.

## 7. Data persistence

### `data.json`

Per-plugin JSON at `<vault>/.obsidian/plugins/<id>/data.json`. (plugin.md §3; VAULT-FORMAT.md §3.)

```typescript
// Plugin lifecycle
const data = await this.loadData();           // returns null on absence
data.foo = "bar";
await this.saveData(data);                    // 2-space pretty-printed JSON
```

**Format** (VAULT-FORMAT.md §3):
- `JSON.stringify(data, undefined, 2)` — 2-space indent, no trailing newline, keys in insertion order.
- Arbitrary plugin-defined shape. Framework enforces nothing.
- Plugin authors typically version via a `schemaVersion` field and migrate in `loadData`.

**Self-edit suppression pattern** (plugin.md §3): `saveData` sets `_lastDataModifiedTime = Date.now()` before the write; adapter writes with same `mtime`. External-edit watcher compares against `_lastDataModifiedTime` and no-ops on self-writes. **Corbomite must preserve this pattern.** Recommended: `libs/pluginhost/PluginDataStore` wrapping `QSaveFile` atomic writes + `QFileSystemWatcher` with 50 ms debounce + mtime-hint comparison.

**External-edit hook.** `Plugin.onExternalSettingsChange()` fires when `data.json`'s mtime advances past `_lastDataModifiedTime`. Plugins supporting hot-reload implement this to re-read `loadData()` and refresh UI.

**Atomic-write improvement.** Obsidian's `adapter.write` is non-atomic (open/write/close). A crash mid-write truncates `data.json`; `readJson` swallows the `SyntaxError` and returns `undefined`, which plugins treat as "no data" → fall back to defaults. **Corbomite should use `QSaveFile`-style atomic rename** — this is strictly better, not a compat break (plugin.md §3).

## 8. Command palette & hotkey surface

### Command shape

See §5.9 `CommandSpec`. Four callback variants: `callback` (always available), `checkCallback` (dispatch-time availability check), `editorCallback` (auto-gate on active `MarkdownView`), `editorCheckCallback` (gate + availability).

**`addCommand` mutates the spec in place** (plugin.md §8). `cmd.id` becomes `"<manifest.id>:<id>"`; `cmd.name` becomes `"<manifest.name>: <name>"`. Plugin authors capturing a reference before `addCommand` see the namespaced form afterward.

### Hotkey binding

Hotkeys bind by **command id**, not by raw callback (core.md §7). This is the reason `hotkeys.json` stores `commandId → Hotkey[]` (VAULT-FORMAT.md §3 "hotkeys.json"). `Keymap.compileModifiers` normalises to `"Meta"` on macOS / `"Ctrl"` elsewhere.

**Corbomite API.** `CommandRegistry` exposes hotkey bindings via `KConfigGroup("Shortcuts")` + `KActionCollection::readSettings`. The on-disk `hotkeys.json` compat layer translates between the namespaced Obsidian form and KDE's native format.

### Built-in namespaces

(core.md §6, workspace.md §6): `app:`, `editor:`, `file-explorer:`, `markdown:`, `theme:`, `window:`, `workspace:`. Plugin commands use `<plugin-id>:`.

## 9. Menu-section protocol

### Emission pattern

Every context menu in Obsidian follows the mid-construction emit pattern (workspace.md §10, ui-bundle.md §1 Menu):

1. Caller builds a `Menu`, calls `menu.addSections([...])` with the canonical order for this menu type.
2. Caller adds built-in items: `menu.addItem(i => i.setSection("close").setTitle("Close"))`.
3. Caller **triggers the workspace event** with the menu mid-construction: `workspace.trigger("file-menu", menu, file, source, leaf)`.
4. Plugin listeners subscribe: `workspace.on("file-menu", (menu, file, source) => { menu.addItem(...) })`.
5. Caller shows the menu: `menu.showAtMouseEvent(evt)`. Sort runs, separators inserted between non-empty sections, submenu configs wrap.

**Plugin items added post-`trigger` are silent no-ops** (ui-bundle.md §8 "Menu.addItem after load"). Plugins that add items inside an async `.then(...)` inside a listener miss the menu entirely.

### Seven menu signals

(workspace.md §4; FEATURE-MATRIX.md §9 "Menu mid-construction plugin hook"):

- `file-menu` — right-click file (source ∈ `"file-explorer-context-menu"`, `"link-context-menu"`, `"more-options"`, `"pane-more-options"`, `"sidebar-context-menu"`, `"tab-header"`).
- `files-menu` — multi-file context (File Explorer only).
- `url-menu` — external link context.
- `editor-menu` — right-click in editor.
- `markdown-viewport-menu` — right-click empty editor viewport.
- `leaf-menu` — tab-header `"…"` button.
- `tab-group-menu` — tab-group chevron-down.

### Corbomite API

```cpp
class Workspace {
signals:
    void fileMenuRequested(QMenu* menu, NoteMeta* file, const QString& source, WorkspaceLeaf* leaf);
    void filesMenuRequested(QMenu* menu, const QList<NoteMeta*>& files);
    void urlMenuRequested(QMenu* menu, const QString& href);
    void editorMenuRequested(QMenu* menu, Editor* editor, MarkdownView* view);
    void markdownViewportMenuRequested(QMenu* menu, MarkdownView*, ViewMode mode, const QString& gutterPath);
    void leafMenuRequested(QMenu* menu, WorkspaceLeaf*);
    void tabGroupMenuRequested(QMenu* menu, WorkspaceTabs*);
};
```

Emit from every right-click handler after built-in `QAction`s are added. Use `MenuSectionRouter` (§5.13) to sort before `exec`.

## 10. Secret storage, network, platform, API version, moment

### SecretStorage

Plugin-accessible keychain wrapper (leaf-utilities.md §1).

```typescript
// Plugin API
app.secretStorage.setSecret(id, value);       // id: /^[a-z0-9-]+$/, ≤ 64 chars
app.secretStorage.getSecret(id);               // returns string | null
app.secretStorage.deleteSecret(id);
app.secretStorage.listSecrets();
app.secretStorage.isEncryptionAvailable();
```

**Corbomite mapping.** `KWallet` (primary) or `QtKeychain` (portable). API shape identical.

### Network

```typescript
// leaf-utilities.md §1
requestUrl(opts): RequestUrlResponsePromise;   // {url, method?, contentType?, body?, headers?, throw?}
request(opts): Promise<string>;                // shorthand for text body
```

**Corbomite mapping.** `QNetworkAccessManager`. **Advantage vs Obsidian:** Qt's `QNAM` is a native HTTP client; there are no browser CORS checks. Corbomite plugins get "CORS-bypass" for free — same behaviour as Obsidian's Electron path.

### Platform

```typescript
Platform: {
  isDesktop, isMobile, isDesktopApp, isMobileApp, isIosApp, isAndroidApp, isPhone, isTablet,
  isMacOS, isWin, isLinux, isSafari,
  canExportPdf, canPopoutWindow, canStackTabs, canSplit, canDisplayRibbon, canPinSidebar,
  supportsIndexedDb, mobileSoftKeyboardVisible, hasPhysicalKeyboard,
  version, build, manufacturer, model, osName, osVersion, deviceName,
  resourcePathPrefix,
}
```

**Corbomite stub.** Mostly constants (desktop-only). Mobile flags all `false`. `isMacOS`/`isWin`/`isLinux` from `QSysInfo::productType()`. Plugin-API shim exposes as an object matching the shape exactly.

### API version

```typescript
apiVersion: string;                    // "1.12.7" as of this audit
requireApiVersion(minVersion): boolean;  // semver compare
```

**Corbomite.** Distinct `corbomiteApiVersion` string for Corbomite-native plugins; may ship a compat layer exposing an `apiVersion` shadowing a subset of the Obsidian API it supports.

### moment

```typescript
import { moment } from "obsidian";
// Re-exports window.moment — already locale-configured via getLanguage()
```

**Corbomite.** Ship `libs/core/MomentFormat` translator (GAP-ANALYSIS §P1.5). Plugin-shim exposes `moment` as a function-compatible object: `moment().format(fmt)` accepts Obsidian's Moment tokens and produces equivalent output via Qt formatters.

## 11. Known compat gotchas (must preserve, even the bugs)

When we implement the plugin API, we do **not** silently "fix" the following behaviours — plugins may depend on them.

### 11.1 `addRibbonIcon` id collision on duplicate title

**Bug** (plugin.md §10). `Plugin.addRibbonIcon(icon, title, cb)` uses `"<manifest.id>:<title>"` as the ribbon item id (NOT `":<iconId>"`). Two calls with the same title collide on `leftRibbon.items`.

**Action.** Replicate. Document. A plugin that calls `addRibbonIcon` twice must use distinct titles or explicitly `removeRibbonAction` first.

### 11.2 `addCommand` mutates its argument

**Behaviour** (plugin.md §8). `cmd.id` and `cmd.name` rewritten in place. A second `addCommand` with the same object produces `"<id>:<id>:<id>"` and an obvious error.

**Action.** Replicate. Document.

### 11.3 Status-bar plugin-id sanitiser single-replacement bug

**Bug** (plugin.md §2). Regex `[^_a-zA-Z0-9-]` applied without `g` flag in `plugin-<sanitised-id>` CSS class. Only the **first** illegal character is rewritten. `a$b$c` → `plugin-a-b$c` (not `plugin-a-b-c`).

**Action.** Replicate. A Corbomite "fix" would silently break plugin CSS selectors.

### 11.4 `EditorSuggest` first-non-null-wins, no priority

**Behaviour** (editor.md §12). Manager iterates suggesters in insertion order; built-ins registered first. Plugin overrides of `[[`/`#` triggers are shadowed, not prioritised.

**Action.** Replicate exactly. Plugin authors who want to replace built-in suggesters must call `unregisterEditorSuggest` on the built-in first — which requires a reference to it. This is by design; a plugin cannot cleanly replace a built-in suggester without forking Obsidian's code.

### 11.5 `activeEditor` setter rejects MarkdownView

**Behaviour** (workspace.md §8). `workspace.activeEditor = someMarkdownView` is a no-op; the setter refuses MarkdownView. The getter falls back to `getActiveViewOfType(MarkdownView)` so it still returns one. Plugins cannot shadow the real MarkdownView with their own.

**Action.** Replicate. Plugins expecting to set `activeEditor` to their own view-type with MarkdownView behaviour will not work — this is by design.

### 11.6 Plugin CM extensions flat-list applied to all editors

**Behaviour** (editor.md §12). `registerEditorExtension` appends to `Workspace.editorExtensions[]`; `updateOptions()` reconfigures **every** `MarkdownView`'s CM `Compartment`. No per-leaf scoping is possible.

**Action.** Corbomite's equivalent `Markoff::EditorPlugin` registry must use the same "one list, applied to every editor instance" semantics. Per-leaf scoping would be a silent compat break.

### 11.7 `MarkdownRenderer.render` without `Component` logs but does not throw

**Behaviour** (editor-markdown.md §8 item 15). Plugins that call `MarkdownRenderer.render(app, text, el, sourcePath, component)` with `component = undefined` get a console warning with plugin-stack detection but the render still completes (leaks event handlers on reload).

**Action.** Corbomite's `MarkoffRenderEngine::render` should require a parent `Component` (or accept `nullptr` with the same console warning). Do not throw; plugins predating the param continue to work.

### 11.8 `registerCodeMirror` is a no-op

**Behaviour** (plugin.md §10, §11). Legacy CM5 shim; `Plugin.registerCodeMirror(cb)` does nothing.

**Action.** Corbomite can omit entirely (Markoff is not CodeMirror). Plugins calling it will see no effect — same as Obsidian.

### 11.9 `EditorSuggest.context` replaced not merged

**Behaviour** (editor.md §12). `EditorSuggest.context` is overwritten on every trigger, not merged. Plugin authors expecting persistence across keystrokes are surprised.

**Action.** Document; replicate.

### 11.10 `Editor.insertText` appends at end-of-doc, not at cursor

**Behaviour** (editor.md §12). Naming gotcha — used by IME-composition-end fast-path.

**Action.** A Markoff `insertText` slot with the same name MUST mirror the end-of-doc semantic for compat. A Corbomite "insert at cursor" method needs a different name.

### 11.11 `editorViewField === editorInfoField` literal reference identity

**Behaviour** (editor.md §8). Two public names for one StateField.

**Action.** Any Corbomite plugin shim that exposes both names must preserve reference identity — updating one updates both. Or document and deprecate one.

### 11.12 `Menu.addItem` after show is a silent no-op

**Behaviour** (ui-bundle.md §8). Plugin listeners that add items post-`menu._loaded` are silently dropped.

**Action.** Replicate. Decision point for Corbomite: mirror (silent drop — compat) or throw (surface the bug). **Recommend mirror** for compat; plugin authors have already written around the behaviour.

### 11.13 `data.json` non-atomic write silently truncates on crash

**Behaviour** (plugin.md §3). `adapter.write` is open-write-close. `readJson` swallows the `SyntaxError` on truncated file.

**Action.** Corbomite can **safely improve** this with `QSaveFile` atomic rename. This is the one case where a fix is compat-safe — plugins don't depend on truncation behaviour; they depend on `loadData()` returning `null`/`undefined` on no-data, which Corbomite preserves.

### 11.14 Plugin-id prefix is mandatory

**Behaviour** (plugin.md §8). `addCommand` unconditionally prepends `manifest.id + ":"`. Accidentally supplying an already-prefixed id yields `"<id>:<id>:cmdId"`.

**Action.** Replicate.

## 12. Out of this sketch (explicit non-goals)

Enumerated so future planning doesn't accidentally add these:

- **Electron IPC APIs.** `ipcRenderer`, `BrowserWindow`, etc. Corbomite uses Qt; plugins depending on Electron APIs will not port. Plugins that call `app.openWithDefaultApp` etc. work via Corbomite's equivalents.
- **Node.js APIs.** `fs`, `path`, `http`, `child_process`, `require('node-module')`. Plugins that import Node modules will not work. Corbomite exposes `requestUrl` and the `DataAdapter` interface as the portable replacements.
- **Direct CodeMirror 6 internals.** `EditorView`, `Decoration`, `StateField`, `ViewPlugin`, `Compartment`. Markoff is not CodeMirror. Plugins shipping CM6 extensions will not port. Corbomite should expose an alternative `Markoff::EditorPlugin` model (see §5.6).
- **Chromium DOM APIs outside the public plugin surface.** `window.electron`, `process.*`, `navigator.serial`, WebGL direct access. Corbomite runs under Qt; many of these are unavailable.
- **`vendor/codemirror` re-exports.** Obsidian re-exports some CM6 symbols via its module; these are not in this sketch.
- **Publish / Sync APIs.** Commercial Obsidian-only services. Out of Corbomite scope.
- **Obsidian mobile platform APIs.** `Platform.isMobile` stubs to `false` forever.
- **Internal-plugin private APIs.** `app.internalPlugins` is a closed registry; plugins cannot add built-ins. Corbomite should mirror.

### Recommended plugin-portability sandbox

When the Corbomite plugin API ships, run plugins in a **QtWebEngine sandbox**. Rationale:

1. Obsidian plugins are JavaScript (or TypeScript compiled to JS). Running them in a JS engine we already have (QtWebEngine) avoids re-implementing the `obsidian` module in C++.
2. The sandbox provides isolation — a plugin crash doesn't bring down Corbomite.
3. DOM APIs are available for plugins that render HTML (Dataview, Tasks). Corbomite's `Markoff::ReadingView` can host the DOM fragment and paint it.
4. The plugin-shim layer (`libs/pluginhost/ObsidianShim`) exposes a JavaScript `obsidian` module implementing §5's registries by proxying to C++ via QtWebChannel.

This is a multi-month implementation. Out of scope for this sketch; flagged here so the overall architectural shape is visible.

## 13. Cross-references

- `plugin.md` — complete enumeration of `Plugin`'s registration verbs with file:line citations.
- `02-extension-surfaces.md` — cross-domain census; includes Pass 2 additions per domain.
- `workspace.md §4, §7, §10` — workspace events, workspace-owned registries, menu-emission protocol.
- `views.md §1, §10` — view hierarchy, subclass entry points.
- `ui-bundle.md §1, §8, §10` — Component, Menu, Modal, SuggestModal, HoverPopover.
- `core.md §1, §4` — Events mixin, Scope, App field catalog, quit/css-change events (emitted on workspace, not app).
- `editor.md §1, §10, §12` — Editor, EditorSuggest, editorExtensions, compat gotchas.
- `editor-markdown.md §10, §7` — MarkdownPostProcessor, MarkdownCodeBlockProcessor, MarkdownRenderChild.
- `settings.md §1, §10` — PluginSettingTab, Setting, SettingTab.
- `metadata.md §10` — MetadataCache plugin-visible shape.
- `bases.md §10` — Bases-plugin registries (registerView, registerGlobalFunc, registerInstanceFunc, operatorFuncConfigs).
- `leaf-utilities.md §1, §10` — moment, Platform, Keymap.isModEvent, requestUrl, SecretStorage.
- `VAULT-FORMAT.md §3` — on-disk files read/written by plugin-framework.
- `GAP-ANALYSIS.md §Cluster N` — plugin-ready surface work sequencing.
- `SHARED-SYMBOLS.md` — short-name resolutions for every plugin-system-adjacent identifier.
