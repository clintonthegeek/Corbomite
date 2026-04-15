# `obsidian/utils` + `obsidian/platform` + `obsidian/secrets` + `obsidian/network` — leaf utility bundle

**Source:**
- `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/utils/`
- `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/platform/`
- `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/secrets/`
- `/home/clinton/bin/ObsidianRAW/audit/renamed/obsidian/tree/obsidian/network/`

**File count:** 19 (14 + 2 + 1 + 2)

**Files:** `utils/apiVersion.js`, `utils/arrayBufferToBase64.js`, `utils/arrayBufferToHex.js`, `utils/base64ToArrayBuffer.js`, `utils/debounce.js`, `utils/getBlobArrayBuffer.js`, `utils/getLanguage.js`, `utils/hexToArrayBuffer.js`, `utils/logException.js`, `utils/moment.js`, `utils/requireApiVersion.js`, `utils/resolveSubpath.js`, `utils/stripHeadingForLink.js`, `utils/stripHeading.js`, `platform/Keymap.js`, `platform/Platform.js`, `secrets/SecretStorage.js`, `network/request.js`, `network/requestUrl.js`

**Pass 1 summary (verbatim from `00-taxonomy.md`):**

> **`obsidian/network` — HTTP request API**
>
> **Files:** 2 (`request.js`, `requestUrl.js`).
>
> **What it does:** Plugin-facing wrappers around node's `http`/`https` (via internal helper) so plugins don't need to hit Electron or Node APIs directly. `request(opts)` returns the body as text; `requestUrl(opts)` returns `{status, headers, text, json, arrayBuffer}`.
>
> **Key exports / primitives:** `request(opts)` — text response, awaits internal helper. `requestUrl(opts)` — full structured response, supports binary.
>
> **On-disk contracts:** none.
>
> **Cross-domain dependencies:** consumed by `plugin`-installed code only.
>
> **Corbomite-relevance note:** Lightly relevant. If Corbomite ever offers a plugin API, plugins should get a `QNetworkAccessManager`-backed equivalent. No Corbomite equivalent today; not needed for vault compat.
>
> **Pass 2 focus:** option-bag schema (headers, body, contentType, throw, method); enough to spec the future Corbomite analogue.

> **`obsidian/platform` — platform detection + keymap**
>
> **Files:** 2 (`Platform.js`, `Keymap.js`).
>
> **What it does:** `Platform` is a flag object (`isDesktop`, `isMobile`, `isMacOS`, `isWin`, `isLinux`, `canExportPdf`, `canPopoutWindow`, `canStackTabs`, `supportsIndexedDb`, `version`, ...). `Keymap` owns the global root scope, modifier-key tracking (`isModifier`, `isModEvent`), and link-open hot-modifier behavior (`pushScope`/`popScope`, `getRootScope`).
>
> **Key exports / primitives:** `Platform` — booleans + getters; `version`, `resourcePathPrefix`, `mobileSoftKeyboardVisible`. `Keymap` — `init()`, `global`, `compileModifiers`, `isMatch`, `isModifier`, `isModEvent`, `pushScope`, `popScope`, `getRootScope`. Hosts the modifier-key drag/click semantics (Ctrl-click to open in new tab, etc.).
>
> **On-disk contracts:** none.
>
> **Cross-domain dependencies:** consumed by basically everything that branches on OS or modifier keys.
>
> **Corbomite-relevance note:** Partial / replaceable. Qt has `QSysInfo`, `QGuiApplication::keyboardModifiers()`, `KStandardShortcut`. Corbomite uses these directly. Plugin-API equivalents would re-export a `Platform`-shaped struct for compat.
>
> **Pass 2 focus:** which getters are read at runtime by other domains (PDF export, stack-tabs, popout-window) — these gate UI features.

> **`obsidian/secrets` — keychain-backed secret storage**
>
> **Files:** 1 (`SecretStorage.js`).
>
> **What it does:** Tiny Electron `safeStorage` wrapper exposed on `App.secretStorage`; plugins (and core sync/Publish) store API tokens here so they don't end up in `data.json`.
>
> **Key exports / primitives:** `SecretStorage` — `get(key)`, `set(key, value)`, `remove(key)`, `isAvailable()`. Backed by OS keychain via Electron.
>
> **On-disk contracts:** stored outside the vault, in OS keychain (Keychain on macOS, libsecret on Linux, Credential Manager on Windows).
>
> **Cross-domain dependencies:** consumed by sync/publish/community plugins.
>
> **Corbomite-relevance note:** Partial / replaceable. Map directly onto `KWallet` (and `QtKeychain` as fallback). Not currently implemented in Corbomite; only relevant once a plugin API exists.
>
> **Pass 2 focus:** the API shape only — adopt verbatim for the future Corbomite wallet wrapper.

> **`obsidian/network`, `obsidian/platform`, `obsidian/secrets`, `obsidian/utils` — leaf utilities**
>
> (As the 13th item in the reading order): leaf utilities consumed by the rest of the app. No vault-format contracts. Quick to audit.

---

**De-minifier artifact note:** All 19 files have distinct md5 hashes — no duplicate extraction. Sixteen of the 19 files are tight extractions (2–80 lines on disk, matching their declared `// source: app.js lines <a>-<b>` range). Two exceptions: `utils/apiVersion.js` declares source lines 221977–225823 (a 3847-line window) but the actual public symbol `apiVersion = "1.12.7"` appears at line 3850 of the file; the 3849 preceding lines are appearance-settings scaffolding (`Zee`, `Jee`, font pickers, theme switcher), leftover extraction bleed from an adjacent code block. Only `apiVersion = "1.12.7"` and the adjacent Electron version constant `yte = "28.2.3"` at line 3851 are in scope for this audit. `secrets/SecretStorage.js` (lines 159325–160472, ~870 lines) contains a similar bleed: the first ~695 lines include callout widget `W1`, `SettingTab`, keychain UI `K1`, and Electron `safeStorage` adapter `r0` and Capacitor adapter `o0`. The actual `SecretStorage` public class begins at line 696; only lines 696–826 are in scope.

---

## 1. Public API surface

### `utils/` sub-directory

#### `apiVersion`

- **Kind:** constant (string)
- **Exported as:** `apiVersion`
- **Signature:** `apiVersion: string` — value `"1.12.7"` (Obsidian 1.12.7 release)
- **Purpose:** The Obsidian API version string. Plugins read this to confirm compatibility; the companion `requireApiVersion` gates features behind it. The adjacent constant `yte = "28.2.3"` is the Electron runtime version (not exported publicly).

#### `requireApiVersion`

- **Kind:** function
- **Exported as:** `requireApiVersion`
- **Signature:** `requireApiVersion(minVersion: string): boolean`
- **Purpose:** Returns `true` when the running Obsidian version satisfies `minVersion`. Internally calls `gy(apiVersion, minVersion)` where `gy` is a semver-style comparator (`utils/requireApiVersion.js:6`). Plugins should gate newer API calls behind this: `if (requireApiVersion("1.4.0")) { /* use new API */ }`.

#### `debounce`

- **Kind:** function
- **Exported as:** `debounce`
- **Signature:** `debounce<T extends (...args: any[]) => any>(fn: T, delay?: number, immediate?: boolean): T & { cancel(): T; run(): void }`
- **Purpose:** Window-aware trailing-edge debounce factory (`utils/debounce.js:4–57`). `delay` defaults to `0`. `immediate = true` switches to leading-edge with timer-extension on repeated calls (the timer is pushed forward to `Date.now() + delay` on each call, so the function fires `delay` ms after the *last* call). The returned debounced function exposes two control methods: `cancel()` clears the pending timer and returns the debounced function; `run()` forces the pending call to fire immediately if one is queued. Multi-window aware: tracks which `window` (`activeWindow`) the timer was scheduled on; if the active window changes between calls and the timer hasn't fired, clears the old window's timer and reschedules on the new one (`utils/debounce.js:38–43`). Used internally by `SecretStorage.saveMeta`, `QueryController.requestNotifyView`, and many UI interactions.

#### `moment`

- **Kind:** constant (Moment.js library reference)
- **Exported as:** `moment`
- **Signature:** `moment: typeof import('moment')`
- **Purpose:** A simple re-export of `window.moment` (`utils/moment.js:5`). Moment.js is loaded globally by Obsidian (bundled in `app.js`). Plugins import `moment` from `obsidian` and get the identical global instance, so locale configuration, timezone plugins, and custom tokens made by the app or by other plugins are already applied. **Format-string parity is critical for Corbomite:** any date-template feature (daily notes, templates, file-name generation) must produce identical output for every format token Obsidian documents. The tokens confirmed in use across Obsidian domains include: `YYYY`, `MM`, `DD`, `HH`, `mm`, `ss`, `ddd`, `Do`, `x` (unix ms), and locale-dependent tokens via `moment.locale(getLanguage())`. Template substitution patterns `{{date:FORMAT}}` and `{{time:FORMAT}}` are handled by the built-in Templates plugin, not directly by this export, but they pass the FORMAT string verbatim to `moment().format(FORMAT)`.

#### `stripHeading`

- **Kind:** function
- **Exported as:** `stripHeading`
- **Signature:** `stripHeading(heading: string): string`
- **Purpose:** Strips display-unfriendly characters from a heading string for use in the UI (not in links). Replaces chars matching `AT = /[!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\r\n]/g` with a space, then collapses multiple spaces and trims (`utils/stripHeading.js:5–7`). The `AT` regex strips most punctuation. Used in `resolveSubpath` to normalise headings for hash-subpath matching (case-insensitive compare after stripping).

#### `stripHeadingForLink`

- **Kind:** function
- **Exported as:** `stripHeadingForLink`
- **Signature:** `stripHeadingForLink(heading: string): string`
- **Purpose:** A narrower stripping pass for use when generating a `[[Note#Heading]]` link fragment. Replaces chars matching `PT = /([:#|^\\\r\n]|%%|\[\[|]])/g` with a space (`utils/stripHeadingForLink.js:5–7`). The narrower set allows most printable punctuation through so that heading links remain readable, while removing only characters that break wikilink syntax (pipe, caret, `%%` comment markers, `[[`/`]]` brackets). Corbomite's link-generation code must apply exactly this stripping before appending `#Heading` to a wikilink.

#### `resolveSubpath`

- **Kind:** function
- **Exported as:** `resolveSubpath`
- **Signature:** `resolveSubpath(cache: CachedMetadata, subpath: string): SubpathResult | null`
- **Purpose:** Resolves a `#heading`, `#^blockid`, or `#[^footnoteId]` subpath to a position range in `CachedMetadata` (`utils/resolveSubpath.js:5–79`). Returns `null` if cache or subpath is falsy. Three dispatch paths by subpath prefix:
  1. **`^blockid`**: lowercase lookup in `cache.blocks`; also checks `cache.listItems` for a matching list-item. Returns `{ type: "block", block, list, start, end }`.
  2. **`[^id]`**: looks up `cache.footnotes` by id. Returns `{ type: "footnote", footnote, start, end }`.
  3. **Heading**: walks `cache.headings` depth-first using `stripHeading(h).toLowerCase()` on each segment. Returns `{ type: "heading", current, next, start, end }` (`end = null` at EOF).
- **Link-format-critical.** This is the canonical implementation called by scroll-to-section, embed rendering, and hover popover. Corbomite must match all three dispatch paths, the case-insensitive matching, and the `end = null` convention exactly.

#### Byte-conversion helpers (group)

Four functions form a symmetric encode/decode family. All are thin wrappers; Qt equivalents are `QByteArray` methods.

| Function | Signature | Notes | Qt equivalent |
|---|---|---|---|
| `arrayBufferToBase64(buf)` | `(ArrayBuffer) → string` | `Mf(new Uint8Array(buf))` — byte-by-byte `btoa`; `utils/arrayBufferToBase64.js:5` | `QByteArray::toBase64()` |
| `base64ToArrayBuffer(b64)` | `(string) → ArrayBuffer` | `window.atob` then byte copy; `utils/base64ToArrayBuffer.js:5` | `QByteArray::fromBase64()` |
| `arrayBufferToHex(buf)` | `(ArrayBuffer) → string` | Two hex chars per byte, lowercase; `utils/arrayBufferToHex.js:5`; used by `Sf` (SHA-256) | `QByteArray::toHex()` |
| `hexToArrayBuffer(hex)` | `(string) → ArrayBuffer` | `parseInt(pair, 16)` per byte; `utils/hexToArrayBuffer.js:5` | `QByteArray::fromHex()` |

All four are purely synchronous. Round-trips are lossless: `base64ToArrayBuffer(arrayBufferToBase64(buf))` restores the original bytes.

#### `getBlobArrayBuffer`

- **Kind:** function (async)
- **Exported as:** `getBlobArrayBuffer`
- **Signature:** `getBlobArrayBuffer(blob: Blob): Promise<ArrayBuffer>`
- **Purpose:** Returns a `Blob`'s contents as an `ArrayBuffer`, preferring the native `blob.arrayBuffer()` method and falling back to `FileReader.readAsArrayBuffer` for older environments (`utils/getBlobArrayBuffer.js:5–26`). Used when reading pasted or dropped file blobs before encoding for storage. Qt equivalent: `QFile` or `QBuffer` read into `QByteArray`.

#### `getLanguage`

- **Kind:** function
- **Exported as:** `getLanguage`
- **Signature:** `getLanguage(): string`
- **Purpose:** Returns the active Obsidian UI language code: reads `localStorage.getItem("language")` first, then falls back to `navigator.language` filtered against Obsidian's supported locale list (`utils/getLanguage.js:5–7`). Drives `moment.locale(getLanguage())`.

#### `logException`

- **Kind:** function
- **Exported as:** `logException`
- **Signature:** `logException(view: EditorView, error: Error, context?: string): void`
- **Purpose:** CM6-specific exception logger (`utils/logException.js:5–12`). Reads the `Mi` (ExceptionSink) facet from the `EditorView`; routes to the first registered sink or falls back to `window.onerror` / `console.error`. This is a CodeMirror integration point; Obsidian has no separate public debug-logging API. `DebugMode` is toggled via `app.loadLocalStorage('DebugMode')` (core domain) but is not surfaced as a plugin API. Corbomite: route through `qCWarning`/`qCCritical`.

---

### `platform/` sub-directory

#### `Platform`

- **Kind:** constant (plain object, mutable at runtime)
- **Exported as:** `Platform`
- **Signature:** (all properties, with computed-property semantics noted)

```typescript
Platform: {
  // Set by platform-detection boot code at startup:
  isDesktop: boolean;           // true = Electron desktop
  isMobile: boolean;            // true = Capacitor mobile
  isDesktopApp: boolean;        // alias for Electron build
  isMobileApp: boolean;         // alias for Capacitor build
  isIosApp: boolean;
  isAndroidApp: boolean;
  isPhone: boolean;
  isTablet: boolean;

  // Derived from navigator.appVersion at module load:
  isMacOS: boolean;             // jl = "macOS" === zl
  isWin: boolean;               // Ul = "Windows" === zl
  isLinux: boolean;             // Wl = "Linux" === zl
  isSafari: boolean;            // _l = UA regex

  // Computed getters:
  canExportPdf: boolean;        // isDesktopApp
  canPopoutWindow: boolean;     // isDesktopApp && isDesktop
  canStackTabs: boolean;        // !isPhone
  canSplit: boolean;            // !isPhone
  canDisplayRibbon: boolean;    // !isPhone
  canPinSidebar: boolean;       // isMobile && !isPhone

  // Set from Electron/Capacitor device info:
  supportsIndexedDb: boolean;   // !!window.indexedDB
  mobileSoftKeyboardVisible: boolean;
  hasPhysicalKeyboard: boolean;
  version: string;              // Obsidian version
  build: string;
  manufacturer: string;
  model: string;
  osName: string;
  osVersion: string;
  deviceName: string;
  resourcePathPrefix: string;   // "file:///" on desktop
}
```

- **Purpose:** Central feature-capability object consulted by workspace (stack-tabs, split, popout), commands (PDF export gate), and UI chrome (ribbon visibility, sidebar pin). The boolean properties `isDesktop`/`isMobile`/etc. are all initialised to `false` and mutated by boot code before any plugin loads, so a plugin reading them in `onload` sees final values. The `isMacOS`/`isWin`/`isLinux` properties derive from `navigator.appVersion` at module load time using string inspection (`zl` derived from `navigator.appVersion`) — they reflect the host OS, not the Obsidian app variant. `platform/Platform.js:5–47`.

#### `Keymap`

- **Kind:** class (singleton via `Keymap.init()`)
- **Exported as:** `Keymap` (and `Scope`)
- **Signature:** Constructor creates a root `Scope`, attaches `keydown`/`focusin` listeners on `window`. `platform/Keymap.js:5–222`.

Key methods (static unless noted):
- `Keymap.init()` — creates or returns `Keymap.global` singleton.
- `Keymap.compileModifiers(mods: string[]): string` — maps `"Mod"` → `"Meta"` (macOS) or `"Ctrl"` (other), sorts, joins with `,`. Stored keybindings always use compiled form.
- `Keymap.decompileModifiers(compiled: string): string[]` — inverse.
- `Keymap.isModifierKey(key: string): boolean` — true for `Control/Alt/Shift/OS/Meta`.
- `Keymap.isModifier(event, mod)` — checks the appropriate `event.*Key` field.
- `Keymap.isMatch(binding, keymapInfo)` — `null` modifiers match any; key compare is case-insensitive with `vkey` fallback.
- `Keymap.isModEvent(event): false | "tab" | "split" | "window"` — middle-click or Mod → `"tab"`; Mod+Alt → `"split"`; Mod+Alt+Shift → `"window"`; no modifier → `false`. Used by every link-click handler.
- `pushScope(scope)` / `popScope(scope)` — instance; modal/menu push their own `Scope` to capture key events.
- `getRootScope()` — instance; returns the root scope.

`Scope` class: `register(modifiers, key, fn)` returns a keybinding ref; `unregister(ref)` removes it; `handleKey(event, info)` walks keys and delegates to `parent` on miss. Returning `false` from a handler causes `preventDefault + stopPropagation`.

---

### `secrets/` sub-directory

#### `SecretStorage`

- **Kind:** class
- **Exported as:** `SecretStorage`
- **Lifecycle:** Instantiated by `App` as `app.secretStorage`; `load()` is called during app startup after `vault` is ready. Not destroyed independently — lives for the app lifetime.
- **Mixes in:** neither `Component` nor `Events`

**Methods (public):**

| Method | Signature | Behaviour |
|---|---|---|
| `load()` | `(): Promise<void>` | Loads `secretsMeta` from `localStorage[n0]`, then calls adapter `load()` to populate `this.secrets`. Shows a `Notice` if secrets exist but encryption is unavailable (and the user hasn't dismissed). |
| `setSecret(id, value)` | `(id: string, value: string): void` | Validates `id` matches `/^[a-z0-9-]+$/` and is ≤ 64 chars; stores in memory and calls `adapter.save(this.secrets)`. Throws if no adapter available or id invalid. |
| `getSecret(id)` | `(id: string): string | null` | Calls `recordAccess(id)` (updates `secretsMeta.lastAccess` at most once per 5 min), then returns `this.secrets[id]` or `null`. |
| `deleteSecret(id)` | `(id: string): boolean` | Removes from `this.secrets`, calls `adapter.save`, returns `true` if existed. |
| `listSecrets()` | `(): string[]` | Returns `Object.keys(this.secrets)`. |
| `isEncryptionAvailable()` | `(): boolean` | `true` unless the adapter is the Electron adapter (`r0`) and `safeStorage.isEncryptionAvailable()` returns `false`. |

**Storage adapters (internal):**
- `r0` (Electron desktop): uses `window.electron.remote.safeStorage.encryptString/decryptString` (OS keychain via Electron's `safeStorage`). Encrypted blob serialised as base64 JSON stored in `localStorage`. Falls back to plaintext if `isEncryptionAvailable() = false`.
- `o0` (Capacitor mobile): uses `U1` (a Capacitor secure storage plugin) — `U1.get/set/remove(key)`.
- `null` (web/dev): no persistence, `setSecret` throws.

**Meta persistence:** `secretsMeta` (last-access timestamps keyed by secret id) is stored in `localStorage[n0]` and saved via a `debounce(..., 5000)` wrapper. `secrets/SecretStorage.js:696–826`.

---

### `network/` sub-directory

#### `request`

- **Kind:** function (async)
- **Exported as:** `request`
- **Signature:** `request(opts: RequestUrlParam | string): Promise<string>`
- **Purpose:** Convenience wrapper — calls the internal `cy(opts)` and returns only the text body (`network/request.js:5–16`). Equivalent to `(await requestUrl(opts)).text`. Suitable when the caller only needs the response body as a string.

#### `requestUrl`

- **Kind:** function (async-like, returns thenable with lazy accessors)
- **Exported as:** `requestUrl`
- **Signature:** `requestUrl(opts: RequestUrlParam | string): RequestUrlResponsePromise`
- **Purpose:** Full HTTP helper for plugins (`network/requestUrl.js:5–7`). Calls `cy(opts)` to get the raw response, then wraps the promise in `uy(promise)` which attaches lazy `arrayBuffer`, `json`, and `text` getters directly on the *promise object* (not the resolved value). This means callers can write `(await requestUrl(url)).text` or `requestUrl(url).text` (the latter accessing the getter that returns a nested promise). `network/requestUrl.js:5–7` + `app.pretty.js:62600–62647`.

**Option bag (`RequestUrlParam`):**

```typescript
interface RequestUrlParam {
  url: string;
  method?: string;           // default: "GET"
  contentType?: string;      // sets Content-Type header
  body?: string | ArrayBuffer;
  headers?: Record<string, string>;
  throw?: boolean;           // default: true — throw on status >= 400
}
```

**Response shape (`RequestUrlResponse`):**

```typescript
interface RequestUrlResponse {
  status: number;
  headers: Record<string, string>;
  arrayBuffer: ArrayBuffer;
  json: any;    // getter: JSON.parse(TextDecoder decode)
  text: string; // getter: TextDecoder decode
}
```

**Error type:** `sy` — subclass of `Error` with `.status: number` and `.headers: Record<string,string>` (thrown when `opts.throw !== false` and `status >= 400`). `app.pretty.js:62496–62508`.

**Internal dispatch (`cy` function):**
1. Mobile (`Em` flag set): forwards to `xm.requestUrl(…)` (Capacitor plugin), body converted to base64 if `ArrayBuffer`.
2. Electron (`If("electron")`): calls `Nf(opts)` — internal Node.js http/https wrapper that bypasses browser CORS restrictions entirely.
3. Web fallback: `fetch(url, { method, headers: { "Content-Type": contentType }, body })`. **This path is subject to browser CORS policy** — a cross-origin request without CORS headers will fail. `app.pretty.js:62524–62597`.

---

## 2. Data structures

### `RequestUrlParam`

```typescript
{
  url: string;
  method?: string;          // defaults to "GET"
  contentType?: string;     // shorthand for Content-Type header
  body?: string | ArrayBuffer;
  headers?: Record<string, string>;
  throw?: boolean;          // default true; false = never throw, return 4xx/5xx as values
}
```

### `RequestUrlResponse`

```typescript
{
  status: number;
  headers: Record<string, string>;   // lowercased header names
  arrayBuffer: ArrayBuffer;          // raw bytes
  json: any;                         // lazy getter
  text: string;                      // lazy getter
}
```

### `SubpathResult`

Three possible shapes returned by `resolveSubpath`:

```typescript
type SubpathResult =
  | { type: "block";    block: BlockCache;    list: ListItemCache | null; start: Pos; end: Pos }
  | { type: "footnote"; footnote: FootnoteCache; start: Pos; end: Pos }
  | { type: "heading";  current: HeadingCache; next: HeadingCache | null; start: Pos; end: Pos | null };
```

`end` is `null` for headings that extend to EOF. All positions use the same `{ line, col, offset }` shape as `CachedMetadata`.

### `SecretsMeta` (internal, stored in `localStorage`)

```typescript
{
  [secretId: string]: {
    lastAccess?: number;   // Unix ms, updated at most once per 5 min
  }
}
```

### `Keybinding` (internal Scope entry)

```typescript
{
  scope: Scope;
  modifiers: string | null;  // compiled, sorted; null = any
  key: string | null;        // null = any key
  func: (event: KeyboardEvent, keymapInfo: KeymapInfo) => boolean | void;
}
```

### `KeymapInfo`

```typescript
{
  modifiers: string;  // compiled modifier string, e.g. "Ctrl,Shift"
  key: string;        // logical key name
  vkey: string;       // virtual key code (e.g. "KeyI")
}
```

---

## 3. On-disk contracts

### `utils/`, `platform/`, `network/`

No on-disk contracts. All utilities are purely in-memory computation or network I/O.

### `secrets/`

Secrets are **not stored in the vault**. Storage locations by platform:

| Platform | Storage location | Mechanism |
|---|---|---|
| Electron desktop (encryption available) | OS keychain via Electron `safeStorage` | Encrypted JSON blob in `localStorage[t0]` |
| Electron desktop (encryption unavailable) | `localStorage[t0]` | Plaintext JSON (warning shown to user) |
| Capacitor mobile | Capacitor Secure Storage plugin key `t0` | Platform keychain (iOS Keychain, Android Keystore) |

`t0` is an opaque `localStorage`/Capacitor key (short name; exact string not derivable from the extraction). `n0` is a separate key for `secretsMeta`. Migration from a legacy key `e0` (old format) is handled in `load()` — reads `e0`, saves under the new key, clears `e0`.

**Secret ID constraints:** `/^[a-z0-9-]+$/`, maximum 64 characters. IDs that fail this regex cause `setSecret` to throw with a localised error message.

---

## 4. Events emitted

No events emitted by any domain in this bundle. None of `utils`, `platform`, `secrets`, or `network` extends `Events` or calls `.trigger(…)`.

---

## 5. Events consumed

### `secrets/SecretStorage`

| Listener | Subscribes to | Why |
|---|---|---|
| `SecretStorage` constructor | `vault.on("create")` — indirectly via `B0` in `workspace`; **not** consumed here | N/A — SecretStorage does not subscribe to vault events |

`SecretStorage` has no event subscriptions. It is a pull-read object — callers invoke `getSecret`/`setSecret` directly.

---

## 6. Commands registered

No commands registered in any file in this bundle.

---

## 7. Registries owned

No registries. `Platform` is a static capability object, not a registry.

---

## 8. Invariants

### `utils/`

- `stripHeading` and `stripHeadingForLink` are pure functions with no side effects and no shared state. They must be applied in exactly the right order when building wikilinks: `stripHeadingForLink` for the `#fragment` portion; `stripHeading` for display and heading-match lookup.
- `resolveSubpath` returns `null` — never throws — when the cache has no headings or the subpath does not match. Callers must null-check.
- `resolveSubpath` heading-match is **case-insensitive** after `stripHeading()` is applied to both the cache heading and the subpath token.
- Block references are matched **case-insensitively** (`r.toLowerCase()`). The block id in `cache.blocks` uses the original casing; the lookup normalises both sides.
- `debounce` with `delay = 0` still defers execution to the next timer tick — it is never synchronous.
- `debounce.run()` is a no-op if no call is pending (no active timer); it does not call the function unconditionally.
- Byte-conversion functions (`arrayBufferToBase64`, `base64ToArrayBuffer`, etc.) are inverses: `base64ToArrayBuffer(arrayBufferToBase64(buf))` round-trips without loss. `arrayBufferToHex`/`hexToArrayBuffer` similarly.

### `platform/`

- `Platform.isMacOS`, `Platform.isWin`, `Platform.isLinux` are derived from `navigator.appVersion` at module load and are **immutable after boot**. Boot code then sets `Platform.isDesktop`/`isMobile`/etc. by mutation before any plugin `onload` fires.
- `Keymap.isModEvent` returns `"tab"` for middle-click (button 1) regardless of modifier state.
- `Keymap.compileModifiers` always sorts modifiers alphabetically — `"Alt,Ctrl"` not `"Ctrl,Alt"`. Stored keybindings must use compiled form.
- `Scope.handleKey` returns `undefined` (falls through to parent) if the matched handler returns `undefined`; returns the handler's value otherwise. Returning `false` causes `preventDefault` + `stopPropagation`.

### `secrets/`

- `setSecret` throws synchronously (no promise) if the adapter is null or the ID fails validation. Callers must not `await` the throw path.
- `getSecret` always records access before returning, including for non-existent keys. The `lastAccess` update is debounced (5 s coalesce) then flushed to `localStorage`.
- `SecretStorage` uses the `debounce` export from `utils/debounce.js` internally for `saveMeta`.
- Secrets are loaded once at startup into `this.secrets` (in-memory dict). All subsequent reads are memory-only; writes call `adapter.save(this.secrets)` synchronously from `setSecret`.

### `network/`

- `requestUrl` with `opts.throw = false` never throws — it always resolves with the response even for 4xx/5xx. Default (`throw` omitted or `true`) throws `sy` for any status ≥ 400.
- On Electron, `requestUrl` bypasses browser CORS. On the web fallback path, CORS rules apply normally. Plugin authors targeting both environments must not assume CORS-bypass.
- `request(opts)` is always equivalent to `(await requestUrl(opts)).text`.

---

## 9. Observable user features

- **`debounce`:** User sees debounced input handling throughout the app — search results appear after typing pauses, settings writes are batched, secret-access timestamps are updated lazily. Users do not interact with `debounce` directly, but it shapes perceived responsiveness.
- **`moment`:** User-visible date strings throughout the app (daily note file names, template `{{date:FORMAT}}` / `{{time:FORMAT}}` substitution, file-creation timestamps in Properties, `MomentFormatComponent` previews in settings) all derive from `window.moment` using the locale set by `getLanguage()`.
- **`resolveSubpath`:** When a user clicks `[[Note#Heading]]` or `[[Note#^blockid]]`, scroll-to-target and embed rendering both use `resolveSubpath` to determine which range to display. If the heading or block does not exist, the link silently fails to scroll (returns null).
- **`stripHeadingForLink`:** When Obsidian generates a wikilink by dragging a heading or using "Copy link to heading", the heading text is run through `stripHeadingForLink` before being appended as `#fragment`. User-visible: headings with pipes or colons produce links with those chars replaced by spaces in the fragment.
- **`Platform` getters:** The user sees PDF export menu items only when `Platform.canExportPdf` is true; popout window actions only when `Platform.canPopoutWindow` is true; ribbon only when `Platform.canDisplayRibbon` is true.
- **`Keymap.isModEvent`:** Ctrl/Cmd-click on any `[[link]]` opens in a new tab; Ctrl/Cmd+Alt+click opens in a split pane; Ctrl/Cmd+Alt+Shift+click opens in a new window. Middle-click always opens in a new tab. This behaviour is universal across the app wherever links are rendered.
- **`SecretStorage`:** Plugins (Obsidian Sync, community plugins such as AI assistants) store API keys via `app.secretStorage.setSecret`. The user sees the Keychain settings tab showing which plugins have stored secrets and the last-access time. If OS encryption is unavailable, the user sees a persistent warning banner.
- **`requestUrl`:** Plugin-initiated HTTP calls (weather widgets, AI completions, sync adapters) bypass Electron CORS on desktop — the user perceives that plugin network requests "just work" without browser security errors.

---

## 10. Extension surfaces exposed

| Surface | Registration verb | Consumer call site | What plugins supply |
|---|---|---|---|
| `debounce` | import from `"obsidian"` | plugin code (no call site in Obsidian itself) | trailing/leading debounce wrapper for their own callbacks |
| `moment` | import from `"obsidian"` | plugin code | date formatting using the app's globally-configured locale |
| `Platform` | import from `"obsidian"` | plugin code to branch on OS/form-factor | read-only capability flags |
| `Keymap.isModEvent` | import from `"obsidian"` | plugin link/click handlers | determine open-in-tab vs split vs window intent from user gesture |
| `app.secretStorage.setSecret/getSecret/deleteSecret` | `app.secretStorage` on `App` | plugin `onload` | opaque string secrets (API keys, tokens) under plugin-chosen ids |
| `requestUrl` / `request` | import from `"obsidian"` | plugin network calls | HTTP requests without CORS restriction on Electron |
| `resolveSubpath` | import from `"obsidian"` | plugin embed/link renderers | resolve `CachedMetadata` + subpath string to position range |
| `requireApiVersion` | import from `"obsidian"` | plugin guards | semver minimum-version check |

---

## 11. Corbomite mapping

### `utils/`

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `debounce(fn, delay, immediate)` | `QTimer` (single-shot, `QTimer::singleShot`) | Have | `QTimer::singleShot(delay, fn)` covers trailing-edge; for the immediate variant with timer-extension, wrap in a helper. `debounce.cancel()` → `timer.stop()`; `debounce.run()` → fire immediately. |
| `moment` / `window.moment` | `QDateTime`, `QLocale`, `KDateTimeWidget` | Partial | Qt has no Moment.js. Date arithmetic is `QDateTime`; format strings are Qt format tokens (`yyyy`, `MM`, `dd`, `hh`, `mm`, `ss`). **Plugin-API compat requires a moment-format-string translator** (e.g. map `YYYY` → `yyyy`, `ddd` → `ddd`, `Do` → custom). Ship a thin `libs/moment-compat/` shim or vendor moment.js (small). |
| `stripHeading(heading)` | Implement in `libs/core/HeadingUtils.h` | Missing | Pure function; straightforward to port. Regex `AT` and `PT` must match exactly. |
| `stripHeadingForLink(heading)` | Implement in `libs/core/HeadingUtils.h` | Missing | Same as above; different regex `PT`. Both needed for correct wikilink generation. |
| `resolveSubpath(cache, subpath)` | Implement in `libs/core/SubpathResolver.h` | Missing | Must match all three dispatch paths (block / footnote / heading) including case-insensitive matching and nested heading depth tracking. |
| `arrayBufferToBase64` / `base64ToArrayBuffer` | `QByteArray::toBase64()` / `fromBase64()` | Have | Direct equivalents. |
| `arrayBufferToHex` / `hexToArrayBuffer` | `QByteArray::toHex()` / `fromHex()` | Have | Direct equivalents. |
| `getBlobArrayBuffer` | `QFile` / `QBuffer` | N/A | No Blob concept in Qt; file reads are direct. |
| `getLanguage()` | `QLocale::system().name()` | Have | Expose as a shim function for plugin API. |
| `logException` | `qCWarning` / `qCCritical` | Have | Route through Qt logging categories; no CM6 facet needed. |
| `apiVersion` | `libs/core/Version.h` compile-time constant | Missing | Corbomite needs its own API version string for plugin compat. |
| `requireApiVersion` | `libs/core/Version.h` + `QVersionNumber::compare()` | Missing | Semver check; straightforward port. |

### `platform/`

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `Platform.isDesktop` | Always `true` | Have | Corbomite is desktop-only. |
| `Platform.isMobile` | Always `false` | Have | — |
| `Platform.isDesktopApp` | Always `true` | Have | — |
| `Platform.isMobileApp`, `isIosApp`, `isAndroidApp`, `isPhone`, `isTablet` | Always `false` | Have | Must be stubbed for plugin compat. |
| `Platform.isMacOS/isWin/isLinux` | `QSysInfo::productType()` | Have | Derive at startup; expose as stub struct for plugin shim. |
| `Platform.canExportPdf` | `true` (Qt has `QPdfWriter`) | Partial | Obsidian gates on `isDesktopApp`; Corbomite can always expose it. |
| `Platform.canPopoutWindow/canStackTabs` | `true` | Have | Qt supports multiple windows. |
| `Platform.version` | `QCoreApplication::applicationVersion()` | Have | — |
| `Keymap` / `Scope` | `KActionCollection` + `QShortcut` + `QKeySequence` | Partial | Qt's shortcut model differs. `Keymap.isModEvent` (Ctrl-click → tab, Ctrl+Alt → split, Ctrl+Alt+Shift → window) must be replicated verbatim in link-click handlers. `Scope.register` ↔ `KActionCollection::addAction`. Push/pop ↔ `QShortcut::setContext(Qt::WidgetWithChildrenShortcut)`. |

### `secrets/`

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `SecretStorage` (desktop: Electron `safeStorage`) | `KWallet` (primary) or `QtKeychain` (fallback) | Missing | `KWallet::writePassword(folder, key, value)` / `KWallet::readPassword(folder, key, value)`. `QtKeychain` (`QKeychain::WritePasswordJob`) works on macOS/Windows/Linux without KDE. Prefer `QtKeychain` for cross-platform portability. The plugin-facing API (`setSecret(id, value)`, `getSecret(id)`) is a thin wrapper; port verbatim including the `id` validation regex `/^[a-z0-9-]+$/` ≤ 64 chars. |
| `SecretStorage.isEncryptionAvailable()` | `QtKeychain` always has OS encryption | Partial | On Linux without a keyring daemon, `QtKeychain` falls back to a file; emit a warning analogous to Obsidian's Notice. |
| Secret ID validation | Same regex in C++ | Missing | `QRegularExpression("^[a-z0-9-]+$")`, max 64. |

### `network/`

| Obsidian concept | Corbomite equivalent | Status | Notes |
|---|---|---|---|
| `requestUrl(opts)` | `QNetworkAccessManager` | Missing | `QNAM::get/post/put/deleteResource(QNetworkRequest)`. The request option bag maps to `QNetworkRequest::setHeader`, `QNetworkRequest::setRawHeader`. Response shape maps to `QNetworkReply`. **Critical plugin-compat gap:** Obsidian's Electron path bypasses CORS entirely; `QNAM` does not perform browser CORS checks (it is a native HTTP client), so cross-origin requests from plugins *do* work without special handling. This is actually a parity advantage — no special CORS bypass is needed. |
| `request(opts)` | `QNAM` + read reply bytes as string | Missing | Convenience wrapper around `requestUrl`. |
| `RequestUrlResponse.json` | `QJsonDocument::fromJson(reply->readAll())` | Missing | — |
| `sy` (`RequestUrlError`) | Custom `QException` subclass with `status` + `headers` | Missing | Throw on status ≥ 400 when `throw != false`. |

---

## 12. Markoff gap confirmations / discoveries

N/A — no editor/rendering surface in this domain.

---

## 13. Open questions

1. **`moment.js` bundle:** Obsidian bundles moment.js in `app.js` and assigns it to `window.moment`. When Corbomite ships a plugin API, will it bundle moment.js for plugins, or require plugins to vendor their own? The plugin-compat argument favours bundling (plugins import `moment` from `"obsidian"` and expect the global). Decision needed before plugin API design.

2. **`logException` — is it a public API?** The file carries `// public API symbol: logException` but its signature takes a CodeMirror `EditorView` as first arg — it is CM6-specific and useless outside a CM extension. Was it exported by mistake, or do Obsidian plugins actually use it in CM extensions? Check community plugin corpus.

3. **`getLanguage()` locale list:** The `Jf` array (supported locales) and `$f` (default locale) are not in the extracted file. What locales does Obsidian support? Relevant if Corbomite wants to match the language-selection dropdown exactly.

4. **`SecretStorage` key names:** `t0`, `n0`, `e0` (localStorage keys) are minifier short names. The actual string values are not recoverable from these extractions. If Corbomite ever needs to migrate secrets from an Obsidian installation (e.g. a user switching to Corbomite), the actual key names would need to be found in a runtime debug session.

5. **`Platform.resourcePathPrefix`:** Hardcoded as `"file:///"` in the Platform object. Is this mutated by boot code for mobile (e.g. to a Capacitor resource path)? Relevant for any plugin that constructs resource URLs.

6. **`requireApiVersion` comparator `gy`:** The implementation is a lexicographic semver-like comparison. Does it handle pre-release suffixes (e.g. `"1.4.0-beta"`)? If Corbomite exports an API version with a suffix, `requireApiVersion` as ported must handle it identically.

7. **`Keymap.isModEvent` on Linux:** On Linux, `Meta` is typically the Super/Windows key. Does Obsidian treat Ctrl as `Mod` on Linux (confirmed: yes, `"macOS" === zl ? "Meta" : "Ctrl"`)? Corbomite's link-click handler should use Ctrl on Linux/Windows and Cmd on macOS.

---

## 14. Recommended Pass 3 synthesis input

1. **`resolveSubpath` + `stripHeading*` are link-format-critical and missing from Corbomite.** These must be implemented in `libs/core/` with exact regex and semantic parity before any wikilink navigation (`[[Note#Heading]]`, `[[Note#^block]]`) can work correctly. Promote to `GAP-ANALYSIS.md` as P1 gap.

2. **`moment.js` format-string parity is required for daily notes / templates.** Corbomite must either bundle moment.js (simplest) or ship a format-token translator (`YYYY`→`yyyy`, `Do`→ordinal, etc.) in `libs/core/`. The `getLanguage()` locale must be wired to the same locale moment uses. Promote to `FEATURE-MATRIX.md`.

3. **`requestUrl` / `request` are the plugin HTTP API.** Qt's `QNAM` is actually a stronger implementation (native HTTP, no CORS restrictions by design) — this is a parity *advantage* not a gap. Promote to `FEATURE-MATRIX.md` as "Corbomite advantage" with the `sy`-error-on-4xx semantic noted for the plugin API spec.

---

## 15. Cross-domain references

| Other domain | Reference type | Brief description |
|---|---|---|
| `core` | dependency | `App.secretStorage` holds the `SecretStorage` instance; `App.loadLocalStorage/saveLocalStorage` are used for secret metadata persistence |
| `core` | sibling | `logException` integrates with CM6's `Mi` facet (defined in CM6/vendor, not core); `getLanguage()` feeds `moment.locale()` used by `MomentFormatComponent` in `ui/components` |
| `metadata` | dependency | `resolveSubpath` operates on `CachedMetadata` (defined in `metadata/`) — its input type crosses domains |
| `vault` | sibling | `ru` (ignored-path predicate), `nu` (basename), `iu` (parent dir), `su` (extension) are defined near `utils/` line ranges in app.js and consumed by `vault/Vault.js`; they are **not** in `utils/` itself but live in the `vault`/internal stratum |
| `plugin` | consumer | `debounce`, `moment`, `Platform`, `Keymap`, `requestUrl`, `request`, `requireApiVersion`, `apiVersion`, `resolveSubpath`, and all byte-conversion helpers are all imported by plugin code via `"obsidian"` package |
| `ui/components` | sibling | `MomentFormatComponent` imports `moment` from this domain; `SecretComponent` (input widget) pairs with `SecretStorage` for in-settings secret editing |
| `workspace` | consumer | `Keymap.isModEvent` is called in workspace link-click handlers; `Platform` capability flags gate workspace actions (PDF export, popout, stack tabs) |
| `rendering` | consumer | `resolveSubpath` is called by the hover-popover and embed-render path to determine scroll target |
| `search` | sibling | `QueryController` uses `debounce` from this domain for `requestNotifyView` |

**Short symbols resolved by this audit (Pass 3 shared-symbol table):**

| Short symbol | Defined in | Used / owned by | Description |
|---|---|---|---|
| `ru` | `vault` / internal (app.js ~35442) | `vault/Vault.js` | Ignored-path predicate: returns `true` if any path component starts with `.`; walks path via `iu` |
| `nu` | `vault` / internal (app.js ~35434) | `vault/Vault.js`, `BL`, `resolveSubpath` | `basename(path)` — last path segment after `/` |
| `iu` | `vault` / internal (app.js ~35438) | `vault/Vault.js`, `ru` | `dirname(path)` — all before last `/` |
| `su` | `vault` / internal (app.js ~35458) | `BL`, `MetadataCache` | `extension(path)` — lowercased extension after last `.` |
| `BL` | `metadata` / internal (app.js ~95373) | `MetadataCache.unresolvedLinks` keys | Linkpath normaliser: strips `.md` extension via `su(nu(path))` |
| `VL` | `metadata` / internal (app.js ~95376) | `MetadataCache.getFirstLinkpathDest` sort | Path-length comparator for disambiguation (shorter path wins) |
| `HL` | `metadata` / internal (app.js ~95381) | `MetadataCache` tag intersection | Set-contains check: does `cache.tags` contain any of the given tag set |
| `zL` | `metadata` / internal (app.js ~95387) | MetadataCache tag validator | Valid tag predicate: non-empty, no newlines, ≤ 100 chars or no spaces |
| `sc` | `metadata` / internal (app.js ~34630) | `MetadataCache.unresolvedLinks`, backlinks | Multi-map class (`Map<string, T[]>`) — `add/remove/get/keys/contains/count` |
| `vb` | `metadata` / internal (app.js ~64083) | `MetadataCache.workQueue` | Serial promise-chain queue — `queue(fn)` appends to a running `Promise.resolve()` chain |
| `wb` | `metadata` / internal (app.js ~64184) | `MetadataCache.linkResolverQueue` | Generator-backed async queue with `addList/add/remove/clear`; backed by `mb` (FIFO deque) and `bb` (cancellable runnable) |
| `Zw` | `metadata` / internal (app.js ~66774) | `MetadataCache._preload` | `idb-open(name, version, { upgrade, blocked })` — wraps `indexedDB.open` as a promise |
| `GL` | `metadata` / internal (app.js ~96726) | `MetadataCache._preload` | Chunked IDB cursor walker: getAllKeys then getAll in slices of `n`, calling `cb(keys, values)` per chunk |
| `Sf` | `utils` / internal (app.js ~42917) | `MetadataCache` content-hash | Async SHA-256 via `crypto.subtle.digest`, result as hex string via `arrayBufferToHex` |
| `kf` | `utils` / internal (app.js ~42897) | `MetadataCache` | `TextDecoder().decode(new Uint8Array(buffer))` — binary buffer to UTF-8 string |
| `Mf` | `utils` / internal (app.js ~42912) | `arrayBufferToBase64` | Byte-by-byte `String.fromCharCode` + `btoa` — the underlying base64 encoder |
| `AT` | `utils` / internal (app.js ~79797) | `stripHeading` | Regex `/[!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\r\n]/g` — wide punctuation strip for display headings |
| `PT` | `utils` / internal (app.js ~79798) | `stripHeadingForLink` | Regex `/([:#\|^\\\r\n]\|%%\|\[\[\|]])/g` — narrow strip for link fragment generation |
| `sy` | `network` / internal (app.js ~62496) | `requestUrl` error path | `class RequestUrlError extends Error { status: number; headers: Record<string,string> }` |
| `B0` | `workspace` / internal (app.js ~163917) | `Workspace.recentFileTracker` | Recent-file tracker: listens to `vault.on("create"/"rename")`, maintains `lastOpenFiles[]`, `serialize/load` for workspace.json |
| `aJ` | `views` / internal (app.js ~153862) | `App.embedRegistry` | `EmbedRegistry` class — maps file extensions to embed factory functions; pre-registers image, audio, video, PDF, markdown extensions |
| `Y6` | unknown domain (app.js ~177920) | unknown | Not resolved in this audit — definition context not read |
