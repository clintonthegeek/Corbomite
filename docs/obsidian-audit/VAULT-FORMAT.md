# VAULT-FORMAT — Obsidian vault on-disk compatibility spec

The canonical on-disk compatibility target for Corbomite. Everything in this document is a contract: implementations must read and write the described bytes exactly, or round-tripping a vault between Corbomite and Obsidian will corrupt data.

The normative sources are the per-domain Pass 2 audit documents under `docs/obsidian-audit/domains/`. Each claim here cites its source so any disagreement can be traced back to the domain doc.

## 1. Scope & principles

**What "vault compat" means for Corbomite:**

- A user can point Corbomite at any Obsidian vault directory (any OS, any Obsidian version ≥ 1.0 that writes the `.obsidian/` config layout) and have it function without corrupting anything.
- A user can modify a vault from Corbomite, close it, and then open it in Obsidian without losing any setting, layout state, plugin data, or note content.
- Vice versa: a vault modified in Obsidian opens cleanly in Corbomite.

**Universal principle — round-trip preserves unknown keys.** Every `.obsidian/*.json` file, every YAML frontmatter block, every `.base` file, and every `workspace.json` node may contain keys or values that Corbomite does not yet understand. These MUST be preserved byte-for-byte on read-modify-write. Obsidian does this via `Object.assign({}, existing, updates)` at every save (vault.md §3 "Unknown keys are preserved on write"); `.base` files explicitly store `unrecognizedData` at every nesting level (bases.md §3, §8). Rationale: any drop-unknown-keys implementation silently strips settings added by future Obsidian versions or by plugins.

**Case-sensitivity probe on vault open.** On desktop, Obsidian detects case-sensitivity by writing `.OBSIDIANTEST` then attempting to read `.obsidiantest` (vault.md §3 "Case sensitivity"). The result is cached on the adapter's `insensitive` field; the name is best-effort-deleted after the probe. Corbomite can adopt the same probe file name (so the two tools don't collide) or pick its own — the filename is not load-bearing. The probe itself is required: without it, `Note.md` vs `note.md` on macOS/Windows produces duplicate `fileMap` entries.

**Path normalisation.** Every path handed to vault/adapter code passes through `normalizePath(s) = tu(uu(s)).normalize("NFC")` (vault.md §1): backslash→slash collapse, slash-run collapse to `/`, trailing-slash trim (except root), leading `./` strip, then Unicode NFC normalisation. Paths are `/`-separated on every OS. Root is `"/"`; all other paths omit the leading slash (vault.md §8). `TFile.extension` is the lowercase ASCII part after the last `.`, no leading dot; `TFile.basename` strips only the last `.` (`Archive.tar.gz` → `basename="Archive.tar", extension="gz"`).

**BOM handling.** `Vault.read` strips a leading U+FEFF before returning; `Vault.modify` never re-adds one (vault.md §8). Corbomite must match — writing back a BOM on a no-BOM read would corrupt Obsidian-edited files.

**Hidden-path filter.** Any path whose component starts with `.` is filtered out of the public `fileMap` (vault.md §8 "Ignored names" via `ru(path)`), except under `configDir` (`.obsidian/` internals). Those fire only `raw` events. Corbomite's vault scanner must match the rule-set so `.git/`, `.DS_Store`, etc. do not appear as notes.

**User-ignore filters.** `app.vault.getConfig("userIgnoreFilters")` is a `string[]` of regex or plain-prefix patterns (metadata.md §3, vault.md §2 `AppConfig`). `MetadataCache` and the file explorer apply these to further filter the effective fileMap. The exact normalisation is: bare strings anchored with `^` and `oc(…)`-escaped; strings wrapped in `/.../` parsed as regex (metadata.md §3 "Inputs consulted"). Invalid regexes log an error but do not crash.

## 2. Vault directory layout

```
<VaultRoot>/
├── <any note or attachment>        (TFile — *.md, *.canvas, *.base, .pdf, images, audio, video, …)
├── .obsidian/                      (configDir — renamable via Vault.setConfigDir, must start with ".")
│   ├── app.json                    (global app + editor + files-and-links settings)
│   ├── appearance.json             (theme + font + accent-colour settings)
│   ├── core-plugins.json           (Record<coreId, boolean>)
│   ├── core-plugins-migration.json (legacy; merged and deleted on first boot)
│   ├── community-plugins.json      (string[] of enabled plugin ids)
│   ├── hotkeys.json                (user hotkey overrides)
│   ├── workspace.json              (desktop layout)
│   ├── workspace-mobile.json       (mobile layout; written only under Platform.isMobile)
│   ├── workspaces.json             (named-workspaces snapshots — owned by Workspaces internal plugin)
│   ├── graph.json                  (Graph-view state — owned by graph internal plugin)
│   ├── bookmarks.json              (Bookmarks plugin data)
│   ├── plugins/                    (community + internal plugin payloads)
│   │   └── <pluginId>/
│   │       ├── manifest.json       (always present; shipped in plugin release)
│   │       ├── main.js             (plugin entry; community plugins only)
│   │       ├── styles.css          (optional; opt-in via Plugin.loadCSS())
│   │       └── data.json           (arbitrary plugin-defined; created on first saveData())
│   └── snippets/                   (user CSS snippets; owned by customCss / Appearance tab)
├── .trash/                         (vault-local trash; created lazily on first trash op)
│   └── <basename><suffix>.<ext>    (desktop suffix " 2", " 3", …; mobile differs)
└── .OBSIDIANTEST                   (transient; written at adapter construction, immediately unlinked)
```

(Sourced from vault.md §3 and cross-referenced across metadata.md §3, workspace.md §3, plugin.md §3, settings.md §3.)

**Attachment folder convention.** `attachmentFolderPath` (app.json key) controls where `Vault.getAvailablePathForAttachments` places pasted or imported attachments:

- `"."` or `"./"` — same folder as the active note.
- `"./sub"` — `sub/` under the active note's folder.
- anything else — vault-absolute path (may be created if missing).

Attachment basename is `stripHeadingForLink`'d and truncated to **250 bytes** (vault.md §1 "getAvailablePathForAttachments", §13 Q11). `getAvailablePath(pathNoExt, ext)` appends ` 2`, ` 3`, … on collision (vault.md §1). The case-insensitive variant is used — `Note.md` cannot be clobbered by `note.md` on case-insensitive filesystems.

**`.trash/` convention.** Desktop writes `.trash/<basename><suffix>.<ext>` preserving the extension; mobile drops the extension separator. Suffix is `""` for the first copy, ` N` for collisions. Never purged by Obsidian — user-managed. Corbomite `trashLocal` must match the desktop form (vault.md §3).

## 3. `.obsidian/` config files

Format invariants common to every `.obsidian/*.json`:

- Written as `JSON.stringify(obj, undefined, 2)` — 2-space indent, no trailing newline, keys in `Object.keys` insertion order (vault.md §3). Corbomite should tolerate any key order on read.
- Absence is valid (all-defaults). No version field in any of these files.
- `config-changed` debounced 500 ms leading-edge when an external edit (git pull, direct edit) is detected; emits `config-changed(key)` per diff (vault.md §4).
- Unknown keys preserved on write (`Object.assign({}, existing, updates)` merge).

### `.obsidian/app.json`

**Purpose.** Global app, editor, and files-and-links settings. The keys that partition into `appearance.json` are routed there via an `AC` allow-list at `saveConfig` time (settings.md §3, vault.md §2; the exact `AC` contents are a follow-up — vault.md §13 Q1, settings.md §13 Q4).

**When created / modified.** First `vault.setConfig(key, val)` call writes both `app.json` and `appearance.json` (debounced 1 s leading-edge via `requestSaveConfig`). Absence = all defaults apply (from `PC[key]` table, a follow-up — vault.md §13 Q2).

**Schema** (vault.md §2 `AppConfig`; field semantics gathered from `setConfig` call sites across Pass 2):

```typescript
type AppConfig = {
  // File & Link (vault-format-critical)
  newFileLocation?: 'root' | 'current' | 'folder';
  newFileFolderPath?: string;              // vault-absolute
  attachmentFolderPath?: string;           // see §2
  alwaysUpdateLinks?: boolean;             // skip the "update links?" prompt on rename
  useMarkdownLinks?: boolean;              // false=[[wiki]], true=[md](url)
  newLinkFormat?: 'shortest' | 'relative' | 'absolute';  // consumed by fileToLinktext (metadata.md §3)
  trashOption?: 'system' | 'local' | 'none';
  promptDelete?: boolean;
  deleteUnlinkedAttachments?: 'always' | 'ask' | 'never';
  showUnsupportedFiles?: boolean;          // affects MetadataCache.isSupportedFile
  userIgnoreFilters?: string[];            // glob/regex patterns, consumed by MetadataCache + fileMap

  // Editor / UI
  defaultViewMode?: 'source' | 'preview' | 'live';
  livePreview?: boolean;                   // seeds editorLivePreviewField StateField (editor.md §3)
  readableLineLength?: boolean;
  showLineNumber?: boolean;
  showInlineTitle?: boolean;
  foldHeading?: boolean;
  foldIndent?: boolean;
  showIndentGuide?: boolean;
  rightToLeft?: boolean;
  spellcheck?: boolean;
  spellcheckLanguages?: string[] | null;
  autoPairBrackets?: boolean;
  autoPairMarkdown?: boolean;
  smartIndentList?: boolean;
  useTab?: boolean;
  tabSize?: number;
  strictLineBreaks?: boolean;              // GLOBAL parser option (editor-markdown.md §12)
  propertiesInDocument?: 'visible' | 'hidden' | 'source';
  autoConvertHtml?: boolean;
  vimMode?: boolean;

  // Workspace chrome
  showRibbon?: boolean;
  showViewHeader?: boolean;
  openBehavior?: 'default' | 'tab' | 'split' | 'window';
  focusNewTab?: boolean;                   // gates getLeaf("tab") auto-focus (workspace.md §8)
  slidingSidebar?: boolean;
  floatingNavigation?: boolean;
  autoFullScreen?: boolean;
  nativeMenus?: boolean;
  uriCallbacks?: boolean;
  baseFontSize?: number;                   // Ctrl+Wheel clamp [10, 30] (editor-markdown.md §3)
  baseFontSizeAction?: 'increase' | 'decrease' | '';

  // Mobile-only
  mobileQuickRibbonItem?: string;
  mobilePullAction?: string;
  mobileToolbarCommands?: string[];

  // Export
  pdfExportSettings?: {
    includeName: boolean;
    pageSize: 'A3'|'A4'|'A5'|'Legal'|'Letter'|'Tabloid';
    landscape: boolean;
    margin: '0'|'1'|'2';                  // "0"=default, "1"=none, "2"=minimal
    downscalePercent: number;             // 10–100
  };
};
```

**Migration.** The only rename observed is `editorFontFamily → textFontFamily` in `setupConfig` (vault.md §2, §3 "Migration/unknown keys"). The old key is deleted unless the new one already exists.

**Unknown-field handling.** Preserved. Do not strip.

**Corbomite gap today.** `.obsidian/app.json` is **not read or written** by Corbomite. Corbomite stores settings outside the vault in `~/.config/corbomiterc` / `~/.config/corbomite-devrc`. This is a P1 compat blocker — see GAP-ANALYSIS §P1 "Vault config I/O". (vault.md §11 entry `Vault.getConfig/setConfig`; settings.md §11.)

### `.obsidian/appearance.json`

**Purpose.** Theme + font + accent-colour settings, routed here from unified `setConfig` via the `AC` allow-list (vault.md §2 `AppearanceConfig`).

**Schema:**

```typescript
type AppearanceConfig = {
  theme?: 'system' | 'obsidian' | 'moonstone' | string;  // string = community theme id
  cssTheme?: string;
  accentColor?: string;                    // hex or ''
  baseFontSize?: number;
  baseFontSizeAction?: 'increase' | 'decrease' | '';
  textFontFamily?: string;                 // migrated from legacy editorFontFamily
  fontFamily?: string;
  monospaceFontFamily?: string;
  interfaceFontFamily?: string;
  editorFontSize?: number;
  translucency?: boolean;
  nativeTitleBar?: boolean;
};
```

(Partial key list confirmed from settings.md §3 per-tab-destination table.)

**Corbomite gap today.** Not read or written.

### `.obsidian/core-plugins.json`

**Purpose.** Enable/disable flags for the 31 built-in ("internal") plugins. Canonical list in core.md §7 "Internal-Plugins registry". Known IDs (confirmed via grep): `"file-explorer"`, `"daily-notes"`, `"sync"`, `"properties"`, `"bases"`, `"global-search"`. The remaining 25 constructors (`P8`, `dJ`, `g9`, `bJ`, `V6`, `i8`, `r7`, `dee`, `q8`, `k7`, `f7`, `v8`, `wee`, `Z8`, `u8`, `k9`, `b8`, `l3`, `U8`, `See`, `w9`, `p7`, `Pee`, `S9`, `a4`, `Iee`, `O8`, `f9`, `uee`, `Z4`, `M2`) have IDs that Pass 3 could not extract in-tree (core.md §13 Q6). Based on published documentation: `"canvas"`, `"backlink"`, `"outline"`, `"graph"`, `"outgoing-link"`, `"tag-pane"`, `"switcher"`, `"command-palette"`, `"bookmarks"`, `"templates"`, `"note-composer"`, `"random-note"`, `"slash-command"`, `"unique-note"`, `"workspaces"`, `"word-count"`, `"zk-prefixer"`, `"page-preview"`, `"editor-status"`, `"audio-recorder"`, `"markdown-importer"`, `"slides"`, `"publish"` (desktop-only).

**Schema (current format)** (settings.md §2 `CorePluginConfig`):

```typescript
// .obsidian/core-plugins.json
{
  [corePluginId: string]: boolean;  // true = enabled, false = disabled
}
```

**Migration.** Older vaults store an array of enabled IDs. On boot, if an array is detected, `InternalPlugins.loadConfig` also reads `.obsidian/core-plugins-migration.json` (shape `{id: {enabled: bool}}`), merges, and rewrites as the object format. After successful migration the `-migration.json` file is deleted (settings.md §2, §3). Corbomite must implement the migration path.

**Unknown-field handling.** Preserve; an unrecognised core-plugin ID simply means "no such plugin registered yet" and its toggle value is remembered for later.

**Corbomite gap.** Corbomite has no internal-plugin model yet; this file is not read/written.

### `.obsidian/community-plugins.json`

**Purpose.** Array of enabled community-plugin IDs.

**Schema** (settings.md §2):

```typescript
string[];  // array of enabled plugin IDs
```

**When created / modified.** First time a user enables a community plugin. Absence = safe-mode (no community plugins load).

**Ordering.** Insertion order (user's explicit enable order); plugin load order may depend on it — settings.md §13 Q5 flags this as needing confirmation. Corbomite should preserve insertion order on round-trip.

**Corbomite gap.** Not read/written.

### `.obsidian/hotkeys.json`

**Purpose.** User overrides for command hotkeys. Default hotkeys (declared in `addCommand({hotkeys: [...]})` at registration time) are **not** persisted — they live in memory. Only overrides appear in this file (settings.md §3).

**Schema** (settings.md §2 `HotkeyConfig`):

```typescript
// .obsidian/hotkeys.json
{
  [commandId: string]: Hotkey[];        // commandId = fully-namespaced "<plugin-id>:<cmd-id>"
                                         // Hotkey[] = the user-assigned combos
                                         // value [] means "no hotkey" (explicitly unbound)
}
type Hotkey = {
  modifiers: ('Mod' | 'Ctrl' | 'Meta' | 'Shift' | 'Alt')[];
  key: string;                           // e.g. "b", "Enter", "ArrowUp"
};
```

**Invariants** (settings.md §3):
- Default hotkeys MUST NOT appear in the file. Writing defaults would mask future Obsidian changes.
- Unknown command IDs on load are preserved (the command may not yet be registered).
- Modifier normalisation via `Keymap.compileModifiers`: `"Mod"`→`"Meta"` on macOS / `"Ctrl"` elsewhere; alphabetised; comma-joined (leaf-utilities.md §1 Keymap). But the stored JSON uses the non-compiled mnemonic array form.

**External-edit detection** (settings.md §3): `vault.on('raw')` filtered to `configDir + '/hotkeys.json'` → `HotkeyManager.load` re-reads. No debounce on write.

**Corbomite gap.** Not read/written. Corbomite uses `KConfigGroup("Shortcuts")` + `KActionCollection::readSettings`.

### `.obsidian/workspace.json` and `.obsidian/workspace-mobile.json`

**Purpose.** Persisted layout of splits, tabs, leaves, sidedocks, floating popout windows, and ribbon state. Most compat-critical artefact in the domain (workspace.md §14 item 1). Desktop and mobile use separate files so they don't interfere.

**Written by** (workspace.md §3): `saveLayout()` debounced 1 s via `requestSaveLayout`; triggered on every split/duplicate/pin/group/tab-select/resize-end/sidedock expand/collapse/popout resize/ribbon reorder.

**Schema — `LayoutJson` root** (workspace.md §2):

```typescript
{
  main?: SplitNode;                       // root tab/split tree for the main window
  left?: SplitNode;                       // left WorkspaceSidedock tree
  right?: SplitNode;                      // right WorkspaceSidedock tree
  floating?: FloatingNode;                // omitted when no popouts open
  'left-ribbon'?: {
    hiddenItems: Record<string, boolean>; // KEY ORDER = runtime item order
  };
  active?: string;                        // active leaf id (16-char random token)
  lastOpenFiles?: string[];               // recentFileTracker; most-recent-first vault-relative paths
}
```

**`SplitNode` discriminated union:**

```typescript
type SplitNode =
  // split container (including sidedock variant)
  | {
      id: string;
      type: 'split';
      direction: 'horizontal' | 'vertical';
      dimension?: number;                 // flex-grow in (0, 100); null/missing = flexible
      children: SplitNode[];
      width?: number;                     // sidedock only (absolute px)
      collapsed?: true;                   // sidedock only
    }
  // tab group
  | {
      id: string;
      type: 'tabs';
      dimension?: number;
      children: SplitNode[];
      currentTab?: number;                // omitted when 0
      stacked?: true;                     // stacked-tab mode
    }
  // leaf (a view instance)
  | {
      id: string;
      type: 'leaf';
      dimension?: number;
      state: ViewState;
      pinned?: true;
      group?: string;                     // linked-pane group id
    }
  // mobile-drawer (mobile only)
  | {
      id: string;
      type: 'mobile-drawer';
      width?: number;
      collapsed?: true;
      children: SplitNode[];
      currentTab?: number;
      pinned?: true;
    }
  // popout window
  | {
      id: string;
      type: 'window';
      direction: 'vertical';              // always vertical after layout restore
      children: SplitNode[];
      x?: number; y?: number;
      width?: number; height?: number;
      size?: { width: number; height: number; x: number; y: number };
      maximize?: boolean;
      zoom?: number;
    };

type FloatingNode = {
  id: string;
  type: 'floating';
  children: Array<{ type: 'window'; ... }>; // non-window children filtered out on deserialise
};
```

**`ViewState` (leaf state payload)** (workspace.md §2, views.md §2):

```typescript
{
  type: string;                           // View.getViewType() or "empty"
  state?: Record<string, any>;            // View-defined (MarkdownView: {file, mode, source?, backlinks?, scroll?})
  icon?: string;                          // cached View.getIcon() for deferred-tab placeholder
  title?: string;                         // cached View.getDisplayText() for deferred-tab placeholder
  active?: boolean;                       // transient; triggers setActiveLeaf on restore
  group?: string;                         // linked-pane group
  pinned?: boolean;
}
```

- `state.file` for `FileView` descendants: vault-relative `/`-separated path.
- `state.mode` for `MarkdownView`: `"source"` or `"preview"`. **Live-preview is NOT a separate mode** — it's `{mode: "source", source: false}`. `source: true` = raw (`**bold**` visible), `source: false` = live-preview with rendered widgets (editor-markdown.md §8 item 2, §2). This is the single most important compat convention.
- `state.scroll` for `MarkdownView` is a **visual-line float** (e.g. `42.73` = "line 42, 73% of the way through"), not a pixel offset; interpolates per-section heights and `li`-offsets. Survives font-zoom and reflow. Corbomite must match for round-trip (editor-markdown.md §8 item 14, §12 "Scroll position is a visual-line float").

**Invariants** (workspace.md §8):
- `WorkspaceItem.id` is a 16-char random token (`cc(16)`); persists across save/load.
- `dimension` sums to 100 among siblings when all are set; `null`/missing = flexible.
- Sibling split `direction` alternates; root `WorkspaceContainer`/`Root`/`Window` always `direction = "vertical"` post-restore.
- Tab-groups with `allowSingleChild = true` never dissolve; splits with `allowSingleChild = false` (default) dissolve at 1 child (remaining child promoted up inheriting `dimension`).
- `['left-ribbon'].hiddenItems` key-order IS runtime item order; `WorkspaceRibbon.load` sorts by `Object.keys(hiddenItems).indexOf(id)`.

**Migration.** No version field. Unknown `type` values cause `deserializeLayout` to return `null` and the parent drops that node — future-version workspace.json silently degrades on older Obsidian. Corbomite must preserve unknown `type` values on round-trip (don't drop, but don't attempt to render either — replace with empty placeholder `tD` if needed, see views.md §1).

**Sandbox-vault special case.** `loadLayout` detects the "Obsidian Sandbox" vault (by name + `ipc("get-sandbox-vault-path")`), detaches every leaf and opens "Start here" — overrides any persisted state (workspace.md §3). Corbomite can ignore; Obsidian-specific.

**Corbomite gap today.** Corbomite stores session state at `~/.local/share/corbomite[-dev]/<vault>/session.json` with a different schema. This is a P1 compat blocker — see GAP-ANALYSIS §P1 "workspace.json compat". (workspace.md §11 entry `.obsidian/workspace.json/workspace-mobile.json`.)

### `.obsidian/workspaces.json`

**Purpose.** Named-workspaces snapshots, one per named workspace. Owned by the Workspaces internal plugin, not the workspace domain itself (workspace.md §3). Switching via `app.internalPlugins.getEnabledPluginById("workspaces").switchToSavedWorkspace(name)` calls `workspace.changeLayout(json)`.

**Schema (inferred).** `Record<workspaceName, LayoutJson>` — one snapshot per named workspace, in the same shape as `workspace.json`. Exact schema is a follow-up (workspace.md §13 Q5).

**Corbomite gap.** Missing entirely. See GAP-ANALYSIS §P2.

### `.obsidian/graph.json`

**Purpose.** Graph-view state (filter + display settings). Owned by the Graph internal plugin. Schema not extracted in-tree — follow-up.

### `.obsidian/bookmarks.json`

**Purpose.** Bookmarks-plugin data. Owned by the Bookmarks internal plugin. Schema not extracted in-tree — follow-up.

### `.obsidian/plugins/<pluginId>/manifest.json`

**Purpose.** Plugin identity. Shipped in the plugin's release archive; plugin framework never writes it, loader reads it once at plugin-discovery and injects `dir` into the resulting `Plugin` instance (plugin.md §2).

**Schema** (plugin.md §2 `PluginManifest`):

```typescript
{
  id: string;                             // unique; used as command prefix and ribbon id
  name: string;                           // human-readable; prefixes command display + CLI handler help
  version: string;                        // semver
  minAppVersion?: string;                 // required minimum Obsidian version
  description?: string;
  author?: string;
  authorUrl?: string;
  fundingUrl?: string | Record<string, string>;
  isDesktopOnly?: boolean;                // loader skips on mobile
  // `dir` is NOT in the file; injected by the loader
}
```

**Required fields for official publication:** `id`, `name`, `version`, `minAppVersion`, `description`, `author`. Enforced at plugin-store submission, not at runtime — a vault with an incomplete manifest still loads.

### `.obsidian/plugins/<pluginId>/data.json`

**Purpose.** Arbitrary plugin-defined settings JSON.

**Written by:** `Plugin.saveData(data)` → `vault.writePluginData(manifest.dir, data, {mtime: Date.now()})` (plugin.md §3). Format: `JSON.stringify(data, undefined, 2)` — 2-space indent, no trailing newline.

**Read by:** `Plugin.loadData()` → returns `null` on absence, `undefined` on parse-fail (swallowed `SyntaxError` logged to console).

**Atomicity.** Obsidian's write is **non-atomic** (open/write/close). A crash mid-write can leave `data.json` truncated. Corbomite should use atomic rename (e.g. `QSaveFile`) — this is an improvement, not a compat break (plugin.md §3 "Atomic-write behaviour").

**Self-edit suppression.** `Plugin.saveData` sets `_lastDataModifiedTime = Date.now()` *before* the write and passes the same timestamp as `mtime` to the adapter. The `Vault.on('raw')` external-edit watcher (debounced 50 ms, scoped to `manifest.dir`) compares against `_lastDataModifiedTime` and no-ops on self-caused writes (plugin.md §3). Corbomite's watcher must mirror: per-plugin file watcher, 50 ms debounce, mtime-hint comparison.

**External-edit hook.** When an external writer (git pull, sync, user) advances `data.json`'s mtime past `_lastDataModifiedTime`, `Plugin.onExternalSettingsChange()` fires (if the plugin overrode it — settings.md §3). Corbomite must wire this.

**Corbomite gap.** No plugin system → no data.json. Reserve the read/write path for when Corbomite's plugin API lands.

### `.obsidian/plugins/<pluginId>/styles.css`

**Purpose.** Plugin stylesheet. Opt-in via `Plugin.loadCSS()` — not automatic (plugin.md §3). No hot-reload; user must disable/re-enable to pick up stylesheet changes.

**Corbomite parity.** Not literally portable (Qt doesn't use CSS). An analogue would be `<plugin-dir>/styles.qss` loaded via a `loadQss()` helper (plugin.md §11).

### `.obsidian/snippets/*.css`

**Purpose.** User CSS snippets. Owned by `app.customCss` (core.md §2 `App.customCss = new Ib(...)`). Each `.css` file is a toggleable stylesheet.

**Schema.** Plain CSS files; each filename becomes a toggle in the Appearance tab. Enabled set persisted under `appearance.json` (key name follow-up).

## 4. Note file (`.md`) on-disk conventions

### 4.1 Frontmatter delimiters

Obsidian identifies frontmatter by matching an opening regex and then a closing regex (parsing.md §1 `getFrontMatterInfo`):

- **Opening:** `Yx = /^---(\r?\n)/g` — the file must start with exactly `---` followed by `\n` or `\r\n`. A BOM before `---` breaks detection; `getFrontMatterInfo` returns `exists: false`.
- **Closing:** `Qx = /---(\r?\n|$)/g` — the closing `---` must be preceded by a `\n`, and may be followed by either another `\n` / `\r\n` OR end-of-file. **EOF-tolerant.**

Return object shape (`FrontMatterInfo`):

```typescript
{
  exists: boolean;
  frontmatter: string;    // raw YAML body between delimiters (excluding them and their newlines)
  from: number;           // byte offset of first char of frontmatter
  to: number;             // byte offset of closing ---
  contentStart: number;   // byte offset of first body char (after closing ---\n)
}
```

**P0 correctness bug in Corbomite.** Corbomite's `Markoff::Document::fromMarkdown` uses `indexOf("\n---", 3)` which requires a preceding `\n`. A file ending with bare `---` (no trailing newline) is not matched. See GAP-ANALYSIS §P0.

### 4.2 YAML parsing and serialisation

**Library:** eemeli/yaml v2. Schema: YAML 1.2 core schema (parsing.md §1 `parseYaml`). Consequences:

- `yes`/`no`/`on`/`off` are NOT booleans (would be in YAML 1.1).
- `true`/`false`/`null` remain special.
- No custom tags beyond core schema.

**`stringifyYaml` options** (parsing.md §1):

```
nullStr: ""                    // JS null → empty YAML value (not "null" or "~")
lineWidth: 0                   // no line-wrapping; long strings stay on one line
aliasDuplicateObjects: false   // duplicate object refs serialised independently; no &/* YAML anchors
```

**Comment preservation.** `stringifyYaml` **does not preserve** YAML comments. Any `processFrontMatter` call that produces a non-empty object drops all comments in the original frontmatter (parsing.md §1 `processFrontMatter`, §3). This is a real data-loss risk; document it for Corbomite users.

**Key order.** JS object property insertion order. `processFrontMatter` preserves order for keys that the mutator does not delete and re-add; added keys append at the end (parsing.md §1, §3). `insertIntoFile` uses `yI` (ordered-assign) — a separate code path; `processFrontMatter` relies on in-place mutation order only.

**Empty-frontmatter rule.** If the mutator returns an object with `Object.keys(fm).length === 0`, the entire `---\n...\n---\n` block is removed from disk. If the file had no frontmatter and the mutated object is non-empty, `"---\n" + yaml + "---\n"` is prepended (parsing.md §1 `processFrontMatter`).

**CRLF.** Detection accepts `\r\n` (opening regex `/\r?\n/`). Writing always uses `\n` — CRLF-originated files round-trip to LF on first write (parsing.md §3 "CRLF-original files get LF delimiters after write-back"). Corbomite should match.

### 4.3 Frontmatter typed helpers

Obsidian exposes four plugin-facing helpers that read typed fields out of a parsed frontmatter object (parsing.md §1):

- `parseFrontMatterEntry(fm, key | RegExp)` — generic getter; returns `fm[key]` if string key; iterates keys for `RegExp`.
- `parseFrontMatterStringArray(fm, key)` — string-or-array → `string[]`, trimming each and filtering non-string elements.
- `parseFrontMatterTags(fm)` — matches `/^tags$/i` (case-insensitive); prepends `#` to any tag without one; drops tags containing spaces.
- `parseFrontMatterAliases(fm)` — matches `/^aliases$/i`; trims and filters empty.

**Invariants** (parsing.md §8):
- Non-string array elements silently dropped (e.g. `tags: [work, 42, null, "home"]` → `["#work", "#home"]`).
- `Tags:` / `TAGS:` accepted (case-insensitive); `tag:` (singular) is NOT.
- Tags containing spaces silently dropped — Obsidian treats them as invalid, not erroneous.
- Stored tags have NO `#` prefix; `#` is injected at read time only.

### 4.4 Body syntax — wikilinks and embeds

Wikilink grammar (parsing.md §1, vault.md §1):

```
[[target]]                              — simple wikilink
[[target|display]]                      — aliased wikilink (| splits target from display text)
[[target#Heading]]                      — heading subpath
[[target#^blockid]]                     — block subpath
[[target#Heading#SubHeading]]           — nested heading path (resolveSubpath walks depth-first)
[[#Heading]]                            — pure subpath (target = "" = current file)
![[target]]                             — file embed
![[target#Heading]]                     — embedded heading section
![[target#^blockid]]                    — embedded block
```

`parseLinktext(linktext)` splits at the **first** `#` — `target` is everything before, `subpath` retains the leading `#`. The `|` alias-split happens in the markdown parser before `parseLinktext` sees the text (parsing.md §1 `parseLinktext`).

**Markdown-link alternative:** when `useMarkdownLinks = true` in `app.json`, Obsidian emits `[display](percent-encoded-target)` instead of `[[target|display]]`. Both syntaxes are always read; only write format is switched (vault.md §1 `generateMarkdownLink`).

**Subpath resolution** (`utils/resolveSubpath.js`, parsing.md §1 `resolveSubpath`): three dispatch paths on prefix:

1. `#^blockid` → case-insensitive lookup in `cache.blocks`; also checks `cache.listItems` for a matching list item. Returns `{type: "block", block, list, start, end}`.
2. `#[^footnoteId]` → `cache.footnotes[id]` lookup. Returns `{type: "footnote", footnote, start, end}`.
3. `#Heading` → depth-first walk through `cache.headings` using `stripHeading(h).toLowerCase()` for case-insensitive compare at each segment. Supports nested `#H1#H2`. Returns `{type: "heading", current, next, start, end}` with `end = null` at EOF.

**Heading text normalisation:**
- `stripHeading(text)` — `AT = /[!"#$%&()*+,.:;<=>?@^`{|}~/\[\]\\\r\n]/g` → space; collapse multi-space; trim. Used for **comparison** (subpath match).
- `stripHeadingForLink(text)` — `PT = /([:#|^\\\r\n]|%%|\[\[|]])/g` → space; same collapse/trim. Used for **generating** the fragment in `[[Note#...]]` links. Strips only link-unsafe chars.

Both functions are pure. Corbomite MUST ship both as `libs/core/HeadingUtils.h` — they are missing today (leaf-utilities.md §11, parsing.md §11).

### 4.5 Embed recursion depth guard

`JZ(containerEl)` walks `.internal-embed` ancestors; `sJ.load({..., depth})` is decremented each step. Prevents infinite recursion on cyclic embeds (editor-markdown.md §12 item 5). Corbomite must implement an equivalent cap — current default is `8` based on observed behaviour, but exact value is a follow-up.

### 4.6 Checkbox round-trip

Rendered DOM pattern (editor-markdown.md §8 item 12):

```html
<li class="task-list-item[ is-checked]" data-task="?" data-line="N">
  <input class="task-list-item-checkbox" type="checkbox"[ checked] data-line="N">
</li>
```

**Preserves non-standard task markers.** `data-task="?"` stores any character found in `[?]` (e.g. `[/]`, `[x]`, `[ ]`, `[>]`, …). Obsidian's click-handler flips `" "` ↔ `"x"` only; other characters flip to `" "`. The canonical helper is:

```typescript
MarkdownRenderer.toggleCheckbox(fullText: string, absoluteLine: number): {text: string; char: string} | null
```

Rewrite regex (verbatim — editor-markdown.md §8 item 12):

```regex
/<li class="task-list-item( is-checked)?" data-task="(.)" data-line="(\d+)"><input class="task-list-item-checkbox" type="checkbox"( checked)? data-line="(\d+)">/g
```

Corbomite's `CheckboxTextObject` (libs/markoff/src/) must implement this verbatim for Tasks/Todo plugin compatibility.

### 4.7 Callouts

Syntax: `> [!type]` at the start of a blockquote line. Optional `+`/`-` suffix enables foldable callouts. DOM structure (editor-markdown/rendering domain):

```html
<div class="callout" data-callout="<type>">
  <div class="callout-title">
    <div class="callout-icon">…SVG…</div>
    <div class="callout-title-inner">…title text…</div>
  </div>
  <div class="callout-content">…body HTML…</div>
</div>
```

Parser produces the AST; renderer produces the DOM (rendering.md §11 "Callout chrome"). Corbomite's `libs/markoff-parser/` emits the AST node; reading-view DOM parity unconfirmed.

### 4.8 `%%comment%%` body syntax

Plain text `%%...%%` delimiters are treated as comments (not rendered). Used freely in note body; not a distinct file-format feature but flagged for cross-tool compat. `stripHeadingForLink` explicitly strips `%%` sequences so they don't leak into link fragments (parsing.md §1 `stripHeadingForLink`).

### 4.9 Attachment basename

`getAvailablePathForAttachments(basename, ext, activeFile)` truncates the basename to **250 bytes** (UTF-8) and `stripHeadingForLink`-normalises it (vault.md §1). Corbomite must match for attachment-paste compat.

## 5. `.canvas` file format

`.canvas` extension registered by the Canvas internal plugin via `ViewRegistry.registerViewWithExtensions(["canvas"], "canvas", factory)` (views.md §7). The Canvas plugin source is not in the Pass 2 scope (plugin/internal-plugins/ is out-of-tree), so the on-disk schema is not defined here.

**Follow-up** (GAP-ANALYSIS §P2): Corbomite has a canvas implementation (`libs/canvas/`, `src/canvas/`); cross-tool compat requires extracting the JsonCanvas schema. The JsonCanvas spec is publicly documented at <https://jsoncanvas.org/>; Corbomite engineers should validate `libs/canvas/` against that spec.

## 6. `.base` file format

`.base` extension registered by the Bases internal plugin for `BasesView` (bases.md §3). Bases is the **largest single feature gap** in Corbomite (estimated 8–10 weeks MVP).

**Format.** YAML at the root, parsed by `parseYaml`. Empty file → default single "Table" view. Non-object root throws `msgErrorInvalidQueryFormat` (bases.md §3).

**Schema** (bases.md §3):

```yaml
# All top-level keys optional. Unknown keys preserved in `unrecognizedData` at every level.

filters:                          # optional global FilterTree
  and:
    - "file.ext == \"md\""
    - or:
        - "note.status == \"open\""
        - "note.status == \"in_progress\""
    - not:
        - "file.tags.contains(\"archive\")"

views:                            # array of named views; defaults to [{type: "table", name: <localised>}]
  - type: table                   # required; "table" or plugin-registered type
    name: "All notes"             # required; unique within views; regex-validated (no # : | ^ [[ ]] %%)
    filters: ...                  # optional view-scoped FilterTree (AND-merged with global)
    order:                        # optional visible-column order
      - file.name
      - note.status
      - formula.priority
    sort:                         # optional multi-key sort; first = primary
      - { property: note.due, direction: ASC }
      - { property: file.name, direction: ASC }
    groupBy:                      # optional single-key grouping
      property: note.status
      direction: ASC
    limit: 100                    # optional row cap; 0 / absent = unlimited
    summaries:                    # optional per-property summary function
      note.amount: sum
    # Free-form view-type-specific options alongside recognised keys:
    image: note.cover             # e.g. for "cards" view
    rowHeight: 120

properties:                       # optional Record<PropertyId, PropertyConfig>
  note.status:
    displayName: "Status"
  formula.priority:
    displayName: "Priority"
    # plus unrecognized keys for forward-compat

display:                          # LEGACY — migrated into properties[].displayName at parse
  note.status: Status

formulas:                         # optional Record<name, expressionString>
  priority: "if(note.urgent, 1, 2)"

summaries:                        # optional Record<name, expressionString>
  totalAmount: "sum(note.amount)"

newItemFolder: "Inbox"
newItemTemplate: "Templates/Task.md"
```

**PropertyId grammar** (bases.md §1 `parsePropertyId` / parsing.md §1):

- `note.<key>` — frontmatter key on the target note; default when no prefix.
- `file.<field>` — file metadata; exactly 14 members in `BasesEntry.FILE_PROPERTIES` (bases.md §8) — closed set.
- `formula.<name>` — named formula defined in the `.base` `formulas:` block.
- Unknown prefix → falls back to `note.<original>`.

**Filter grammar** (bases.md §3):

Every node is one of:
- `{and: [<filter>, …]}` / `{or: [<filter>, …]}` / `{not: [<filter>, …]}` — empty lists collapse via `optimize()`.
- A **string** formula expression parsed at load time. Example: `"note.status == \"open\""`.

**The formula/filter DSL is not defined inside `obsidian/bases/`.** The `DK` parser lives in adjacent app.js code (the Bases internal plugin). Operators, functions, precedence — all are follow-ups (bases.md §13 Q1). User documentation exists at <https://help.obsidian.md/bases/functions>.

**Migration** (bases.md §3):
- No `version` field.
- Forward-compat via `unrecognizedData` preservation at every nesting level.
- Legacy `display:` → `properties[propId].displayName` at parse; new writes use only `properties:`.
- Empty file → default 1-view "Table" query.

**Validation errors** (bases.md §3): `BasesQuery.parse` throws localised errors for non-object root; `views` not array; `properties`/`display` not object; `display` values not strings; `formulas`/`summaries` not object or non-string values; `newItemFolder`/`newItemTemplate` not strings.

**Invariants** (bases.md §8, `[CRIT]` = required for compat):

- `[CRIT]` Top-level YAML object.
- `[CRIT]` Empty `.base` is valid.
- `[CRIT]` Unknown keys round-trip verbatim.
- `[CRIT]` `views` order matters. `views[0]` is default when `getViewConfig(undefined)`.
- `[CRIT]` View name uniqueness (validator enforces on editor; tolerate duplicates on read).
- `[CRIT]` PropertyId grammar closed for `file.*`; `note.*` case-insensitive key lookup; persisted casing preserved.
- Sort direction strings uppercase (`"ASC"`/`"DESC"` only).
- `limit: 0` means unlimited (not null, not absent).

**Corbomite gap.** Missing entirely. See GAP-ANALYSIS §P2 "Bases".

## 7. Link resolution algorithm

This is the central cross-tool compatibility contract. A user's `[[Foo]]` link resolves to a specific `TFile`; Obsidian's rule produces a deterministic answer, and any cross-compatible tool MUST produce the same answer.

**Algorithm** (`MetadataCache.getLinkpathDest` — metadata.md §8 "Link resolution algorithm"):

Inputs: `linktext: string`, `sourcePath: string`. Output: `TFile[]` in precedence order (first entry wins).

1. **Empty linktext + sourcePath** → return `[vault.getAbstractFileByPath(sourcePath)]` if it's a file, else `[]`.
2. **Name lookup.** Lowercase the linktext. If it contains `.`, look up `uniqueFileLookup[name]` (a multi-map `lowercaseFilename → TFile[]`). Else retry with `name + ".md"` lowercased. No candidates → `[]`.
3. **Exactly one candidate and linktext matched with its literal extension** → return it alone.
4. **Relative `./` or `../`.** Strip leading `./../`, walk `../` up the source's parent chain, compare each candidate's lowercased path for literal equality against the resolved absolute. Exact match → `[match]`. Miss → fall through.
5. **Leading `/` — rooted absolute only.** Strip slash; require exact path match. If nothing matches and the original started with `/`, return `[]` — **do not attempt short-name disambiguation**.
6. **Short-name disambiguation.** Partition candidates whose lowercased path `endsWith(linktext)` into `sameFolder` (starts with source's lowercased folder prefix + `/`) and `otherFolder`; sort each by `VL` (shortest-path, alpha tiebreak) and return `sameFolder.concat(otherFolder)`.

**Summary: shortest-path-wins, with same-folder preference.**

**Inverse operation.** `fileToLinktext(file, sourcePath, newLinkFormat)` renders a `TFile` into the minimal linktext that resolves back (metadata.md §8). Under `"shortest"` it tries `basename` first; widens to full path if `getLinkpathDest(basename, sourcePath) !== [file]`.

**Subpath extraction.** Before running resolution, `getLinkpath(linktext)` strips `#heading`/`#^block` — the resolver operates on the path portion only. Corbomite's current wikilink regex (`SQLiteIndex.cpp:462–464`) does NOT strip `#subpath` — it sends `"Note#Heading"` to the target column, breaking both resolution and scroll-to-subpath navigation (parsing.md "Pass 2 additions"; 01-markoff-gaps.md §parsing).

**`unresolvedLinks` key normalisation.** Keys are passed through `BL(getLinkpath(linktext))` before storage: lowercased, heading/block-id stripped (metadata.md §8). Plugins reading `unresolvedLinks` expect lowercase keys without `#` suffixes.

**P0 correctness gap.** Corbomite's `SQLiteIndex::resolveWikilink` (libs/storage/src/SQLiteIndex.cpp:592–609) implements only steps 1-2 flatly — no shortest-path tiebreak, no same-folder preference, no relative `./`/`../` support, no rooted-absolute check. Under ambiguity it returns whichever copy was hashed last. See GAP-ANALYSIS §P0.

## 8. Attachment handling

Covered in §2 "Attachment folder convention" and §4.9 "Attachment basename". Key points:

- `attachmentFolderPath` app.json key configures destination policy.
- `stripHeadingForLink` sanitises basename; truncated to 250 bytes.
- `getAvailablePath` appends ` 2`, ` 3`, … to disambiguate on collision (case-insensitive).

**Image download.** `FileManager.downloadAttachmentsForNote(file)` scans markdown + frontmatter for `http(s)://` and `data:` image URLs, prompts the user, downloads via `requestUrl`, saves as attachments, rewrites the note (vault.md §1).

## 9. Case-sensitivity, Unicode, Windows path handling

**NFC normalisation.** Every path handed into the vault API is NFC-normalised (vault.md §8). This matters on macOS (HFS+ uses NFD natively), on Linux (users can store NFD filenames), and for cross-tool round-trip. Corbomite must apply NFC at every library boundary.

**Path separators.** Internal representation is always `/`. The desktop adapter translates to host separators via Node's `path.join` on Windows (vault.md §3).

**Case-sensitivity.**
- Probe runs once per session (`.OBSIDIANTEST`).
- `Vault.getAbstractFileByPathInsensitive` first does an exact lookup, then a case-insensitive scan across equal-length keys (vault.md §1).
- `Vault.getAvailablePath` uses `getAbstractFileByPathInsensitive`, so `Note.md` cannot be clobbered by `note.md` even on case-insensitive FS.
- `Vault.rename` allows case-only renames on insensitive FS by short-circuiting the exists check (vault.md §1).
- Cross-vault interop: if a vault on case-insensitive FS has two notes whose lowercased paths collide, one of them becomes inaccessible on a case-sensitive FS (e.g. Linux). Obsidian does not attempt to resolve this; Corbomite should surface a warning on vault open.

**Windows UNC paths.** `FileSystemAdapter.getResourcePath` prefixes `%5C%5C` for UNC paths to survive URL encoding (vault.md §1). `app://local/<file-url>?<mtime>` is the resource URL scheme — the `?<mtime>` query bust-cache suffix is invariant (bases.md §12 "ImageValue").

**Illegal filename chars.** Validated by regex sets `UT`/`WT`/`GT`/`KT` (vault.md §13 Q3) — exact sets are a follow-up.

## 10. IndexedDB metadata cache (host-side, NOT on-disk)

Obsidian does NOT write a vault-local metadata cache file. The cache lives in the browser's IndexedDB:

- **DB name:** `<app.appId>-cache` (metadata.md §3).
- **DB version:** 19 (as of Obsidian 1.12.7).
- **Object stores:** `"file"` (key = vault-relative path, value = `{mtime, size, hash}`), `"metadata"` (key = SHA-256 content hash, value = `CachedMetadata` with `frontmatterPosition` renamed to `frontmatterPos` on disk).
- **Durability:** `{ durability: "relaxed" }` — writes may linger in the browser buffer through a crash.
- **Version-bump migration:** any version < 19 → both stores dropped and recreated. No migration path; user re-indexes the vault.

**This means the cache is a host concern, not a vault-disk contract.** Moving a vault between machines or tools triggers a full re-index. Corbomite's `libs/storage/SQLiteIndex` persists to `~/.local/share/corbomite[-dev]/index.sqlite` (outside the vault), which mirrors this invariant. Both tools are free to use their own cache implementation as long as they do not pollute the vault root with cache files.

**`CachedMetadata` shape** (metadata.md §2) — this is what IndexedDB stores and what plugins read via `metadataCache.getFileCache(file)`:

```typescript
{
  links?: LinkCache[];
  embeds?: LinkCache[];
  tags?: TagCache[];
  headings?: HeadingCache[];
  sections?: SectionCache[];
  listItems?: ListItemCache[];
  footnoteRefs?: { id: string; position: Position }[];
  footnotes?: { id: string; position: Position }[];
  blocks?: Record<string, { id: string; position: Position }>;
  frontmatter?: Record<string, unknown>;
  frontmatterLinks?: FrontmatterLinkCache[];
  frontmatterPosition?: Position;          // renamed to frontmatterPos on disk
}

interface LinkCache {
  link: string;                            // raw linkpath as parsed (pre-getLinkpath)
  original: string;                        // verbatim "[[foo|bar]]" or "[x](y)"
  displayText?: string;                    // alias if any
  position: Position;                      // start/end with {line, col, offset}
}

interface FrontmatterLinkCache {
  link: string;
  original: string;
  displayText?: string;
  key: string;                             // dotted path like "project" or "related.0"
}

interface Pos { line: number; col: number; offset: number; }
interface Position { start: Pos; end: Pos; }
```

Corbomite's plugin-compat shim (once Corbomite gains a plugin API) must expose this shape byte-for-byte — plugins read these objects directly. See GAP-ANALYSIS §P3 "`CachedMetadata` exposure".

## Implementation additions — 2026-04

Following Corbomite-side implementation, the following two `.obsidian/` config files were added to the living set that Corbomite reads/writes. Full schemas in `addenda/2026-04-15-daily-notes-templates-schemas.md`:

- **`.obsidian/daily-notes.json`** — Daily Notes internal-plugin config. Keys: `format` (Moment date-format), `folder` (vault-relative), `template` (vault-relative path, optional), `autorun` (bool). Round-trip via `Corbomite::VaultConfig::readDailyNotesJson` / `writeDailyNotesJson`. Unknown keys preserved.
- **`.obsidian/templates.json`** — Templates internal-plugin config. Keys: `folder` (vault-relative), `date_format` / `time_format` (Moment formats). Round-trip via `Corbomite::VaultConfig::readTemplatesJson` / `writeTemplatesJson`. Unknown keys preserved.

Both surfaced during Cluster F. Corbomite now produces Obsidian-parity `{{date:FMT}}` / `{{time:FMT}}` template substitution via `Corbomite::MomentFormatter` (hand-translator covering YYYY, MMM, MMMM, Do, dddd, ww, h, HH, a, A, [literal], etc.).

## 11. Open format questions / follow-ups

Consolidated from per-domain §13 Open Questions. These are blockers for specific narrow features but not blockers for the primary vault read/write path.

### Must-extract from full `_internal.js` / bundled code

1. **DOMPurify allowlist (`SL`)** (rendering.md §13 Q1). Security-boundary critical. Without the exact allowlist, plugin-HTML sanitisation diverges from Obsidian. Recommended action: grep the full `app.js` / `_internal.js` for `ALLOWED_TAGS` and `ALLOWED_ATTR` configuration.

2. **Turndown rule set (`hP`)** (rendering.md §13 Q2). Web-clipper and paste-from-browser compat. Grep for `new TurndownService` / `.addRule(`. Includes GFM extensions.

3. **Bases filter/formula DSL (`DK` parser)** (bases.md §13 Q1). Operators (`==`, `contains`, `matches`, `in`, …), functions (`if`, `sum`, `count`, `link`, `date`, …), precedence. User docs at <https://help.obsidian.md/bases/functions> are the authoritative user-facing grammar.

4. **Search-panel DSL (global-search plugin)** (search.md §13 Q1). `tag:`, `path:`, `file:`, `line:`, `block:`, `section:`, `/regex/`, `"quoted"`, `-exclusion`, `match-case`, `whole-word`, AND/OR/NOT. Lives in `plugin/internal-plugins/global-search/` — not in the Pass 2 scope.

5. **`AC` (appearance-keys allow-list) contents** (vault.md §13 Q1, settings.md §13 Q4). Determines which keys partition into `appearance.json` vs `app.json` at write time.

6. **`PC` (vault-config defaults table) contents** (vault.md §13 Q2). Every key in `AppConfig` needs its default documented.

7. **Illegal-filename regex sets `UT`/`WT`/`GT`/`KT`** (vault.md §13 Q3).

8. **Internal-plugin manifest IDs** for the 31 built-in constructors (core.md §13 Q6).

9. **`.obsidian/workspaces.json` schema** (workspace.md §13 Q5).

10. **`.obsidian/graph.json` schema.**

11. **`.obsidian/bookmarks.json` schema.**

12. **`.canvas` schema** — extract from <https://jsoncanvas.org/> or from the canvas internal plugin when it enters the audit scope.

### Format-critical behavioural questions

13. **Footnote frontmatter cache shape** (metadata.md §13 Q5) — `footnoteRefs` vs `footnotes` inferred from plugin lore; confirm against actual worker output.

14. **CRLF preservation on external-edit merge.** `TextFileView.save` writes `\n`; the three-way-merge path is silent on CRLF. Files that round-trip through an external CRLF editor lose CRLF on next Obsidian save (parsing.md §3). Document for Corbomite.

15. **IndexedDB `frontmatterPosition` persistence format.** `vT` / `mT` / `gT` are position-pruning helpers (metadata.md §13 Q2). Not a vault-on-disk concern since cache is host-side, but relevant if Corbomite ever reads an Obsidian cache.

16. **Case-only rename on case-insensitive FS.** Confirmed Obsidian allows it by short-circuiting exists check (vault.md §1). Corbomite's `FileSystemAdapter` must match.

17. **`.obsidian/` recursive watcher behaviour on Linux** (vault.md §3). `QFileSystemWatcher` is inotify-backed; Obsidian installs per-directory watchers manually for dotfiles. Corbomite must match.

18. **Webview resource URL cache-bust.** `app://local/<path>?<mtime>` query string. Invariant — omitting `?<mtime>` means stale cache. Corbomite's attachment rendering must match (bases.md §12 "ImageValue").
