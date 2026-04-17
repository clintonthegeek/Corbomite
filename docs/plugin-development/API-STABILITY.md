# API stability contract

This document is the promise the Corbomite project makes to
plugin authors about what will and will not break under your feet.

If you are evaluating whether to invest time in a Corbomite plugin,
read this page before committing.

---

## Two levels of stability

### Shape-stable (from Cluster N close, today)

The plugin API is **shape-stable**: no method removals, no signal
removals, no semantic changes to existing surfaces. Additive changes
(new proxy methods, new permission tokens, new signals, new
lifecycle hooks with default empty implementations, new registry
types) are always allowed. Minor ergonomic revisions — argument
defaults, method overloads that do not break existing callers — are
allowed. Breaking changes in this window are announced under "Plugin
API adjustments" in `CHANGELOG.md`.

This is the commitment we make immediately so third-party authors
can start building without fearing a breaking change lands tomorrow.

### 1.0-frozen (future, not yet)

Once real third-party plugins have shaken out the ergonomics of the
surface, Corbomite will declare the plugin API **1.0-frozen**. At
that point the full 12-month deprecation rule applies to every
change, including ergonomic tweaks. The bump to 1.0 is a separate
event — not part of Cluster N — and will be announced when it
happens.

**Why not 1.0 today?** Because we have not yet observed what third-
party plugins actually want. The reference plugin
(`examples/note-stats-plugin/`) exercises a good slice of the surface
but is not a substitute for real community consumption. A 1.0 stamp
made before we see ergonomic friction would be either dishonest (we
would break it anyway) or paralysing (we would freeze awkward shapes
forever). Instead we ship shape-stable now and freeze when the
evidence justifies it.

---

## What is covered

The following surfaces are shape-stable from Cluster N close:

- Every public method of the proxy classes listed in
  [API-REFERENCE.md](API-REFERENCE.md): `VaultProxy`,
  `FileManagerProxy`, `MetadataCacheReader`, `SearchProxy`,
  `WorkspaceController`, `CommandRegistrar`, `ViewRegistrar`,
  `MenuInjector`, `SecretStorage`, `ProcessSpawner`.
- Every signal listed on `VaultProxy`, `MetadataCacheReader`, and
  `WorkspaceController`.
- The 12 permission tokens currently declared. New tokens may be
  added; existing tokens' semantics will not change.
- The `Plugin` virtual lifecycle hooks: `onLoad(PluginContext*)`,
  `onUnload()`, `createView(MainWindow*)`, `focus(QObject*)`,
  `saveSessionState(QObject*)`, `loadSessionState(QObject*,
  QJsonObject)`, `configPages()`, `configPage(int, QWidget*)`.
- `PluginContext` accessors (`vault`, `fileManager`,
  `metadataCache`, `search`, `workspace`, `commands`, `views`,
  `menus`, `network`, `secrets`, `process`, `config`, `loadData`,
  `saveData`, `metaData`, `grantedPermissions`, `hasPermission`).
- The `metadata.json` schema: `KPlugin` keys plus the five
  `X-Corbomite-*` keys (`Trusted`, `Permissions`, `DockArea`,
  `MinVersion`, `ApiLevel`).

---

## What is NOT covered

The following surfaces remain unstable and may change without
deprecation. Plugins must not depend on them.

- `libs/core/` internal types not reachable through a proxy:
  `Component`, `Events` mixins, `CachedMetadata`, etc.
- Direct types: `Vault`, `FileManager`, `MetadataCache`,
  `SQLiteIndex`, `NotesTreeModel`, `Workspace`, `WorkspaceLeaf`,
  `MainWindow`. Plugins receive these as opaque `QObject *` or not
  at all.
- Widget class hierarchies beyond `View`. Plugins instantiate their
  own `QWidget` subclasses; they do not subclass Corbomite's
  non-`View` widget classes.
- Markdown rendering internals: `MarkdownEditor`, `ReadingView`,
  the embed / codeblock-processor registries' internal shape
  (the registries themselves are reachable via proxies, but the
  renderer pipeline is not stable).
- Any `#include` path starting with `corbomite/core/` or
  `corbomite/vault/` that is not under `proxies/` and not one of
  `Plugin.h`, `PluginContext.h`, `PluginApi.h`, `Command.h`.

If you find yourself reaching for an unstable type, open an issue.
The right resolution is almost always a new proxy method or
registry hook, not a plugin taking a dependency on the unstable
surface.

---

## Version compatibility

Two mechanisms guard plugins against host/plugin mismatches:

### `X-Corbomite-MinVersion`

Declared in your plugin's `metadata.json` as a semver-compatible
string. `PluginManager` reads it at discovery. If the plugin's
declared minimum exceeds the host's
`QCoreApplication::applicationVersion()`, the plugin is refused at
enable time with the `PluginsPage` state "Requires Corbomite >= X"
and a `qCWarning` emitted.

Set this to the Corbomite release you tested against. It is human-
facing; a newer plugin targeting an older host gets a clear refusal
rather than a runtime crash.

### `X-Corbomite-ApiLevel`

Declared as an integer in `metadata.json`. Defaults to `1` when
absent. Semantics:

- The host publishes `constexpr int CORBOMITE_PLUGIN_API_LEVEL` in
  `corbomite/core/PluginApi.h`. Today it is `1`.
- A plugin that declares `X-Corbomite-ApiLevel <= CORBOMITE_PLUGIN_API_LEVEL`
  loads. If the plugin omits the key, it is treated as declaring
  level `1` and loads against today's API.
- A plugin that declares a level *higher* than the host supports is
  refused at enable time with "Requires plugin API level >= N"
  and a `qCWarning`.

`ApiLevel` is deliberately independent of the human-readable
`MinVersion` so Corbomite can make a hard ABI break without having
to coordinate with version-number cosmetics. When we bump the
level, compatibility shims for the previous level stay in place for
one major Corbomite version.

---

## How we evolve the API

### Additive changes — always allowed

- New proxy methods.
- New permission tokens. Plugins that do not declare a new token
  are unaffected; the new token gates only the new methods that
  require it.
- New signals on existing proxies.
- New lifecycle hooks on `Plugin`, always with default empty
  implementations so existing plugins do not need to override.
- New registry types reachable via new `PluginContext` accessors.
- New `X-Corbomite-*` metadata keys with defined defaults when
  absent.

Additive changes land in any release; no deprecation is required
because they cannot break existing plugins.

### Deprecations — 12-month window (post-1.0)

Once the API is 1.0-frozen:

- The deprecated declaration is annotated with
  `[[deprecated("use X instead")]]` so compilers flag callers.
- The runtime emits `qCWarning` on the first call per session
  under a dedicated category (e.g. `corbomite.plugin.deprecated`),
  so plugin authors see it in logs even when they ignore compiler
  warnings.
- The deprecated surface continues to function for at least 12
  calendar months after the deprecation is announced.
- `CHANGELOG.md` records the deprecation and its target removal
  version.

After the 12-month window, removal is allowed in the next release.
Breaking-change notes land in the `CHANGELOG.md` entry for that
release plus a dedicated "Breaking changes" section in this file.

### Removals — post-window only

After the 12-month window passes, a deprecated method may be
removed. Plugins still depending on it were warned for a year; the
burden is on them to migrate.

### Major rewrites — bump `ApiLevel`

If a change cannot be shoehorned into the additive + deprecation
model — for instance, because it has to change the semantics of an
existing method, or because a security fix requires an immediate
break — Corbomite bumps `CORBOMITE_PLUGIN_API_LEVEL`. Plugins
declaring the old level (or omitting the key, which defaults to `1`)
continue to load against a compatibility shim for one major
Corbomite version. Plugins declaring the new level get the new
surface. At the end of the shim window, the old level is dropped.

This is the break-glass. We do not intend to use it lightly.

---

## Pre-1.0 caveats

In the window between Cluster N close and the eventual 1.0 freeze:

- **Ergonomic tweaks may land without the 12-month window.** If
  real plugin authors hit a sharp edge (a `QString` that should
  be a `QStringView`, a method that should return `std::optional`,
  a signal parameter we got wrong), we may adjust the shape.
  Adjustments of this kind are announced in `CHANGELOG.md` under
  "Plugin API adjustments" with migration guidance.
- **Permission tokens may be split, merged, or renamed.** The
  current set (`vault.read`, `vault.write`, `vault.events`,
  `metadata.read`, `workspace`, `ui.commands`, `ui.views`,
  `ui.menus`, `network`, `secrets`, `process`, `config`) is
  expected to be stable, but an early adopter may surface a case
  where the grain is wrong. If that happens, we adjust once,
  document the shift, and freeze the new set at 1.0.
- **The 1.0 freeze date is not scheduled.** It happens when the
  surface has absorbed enough real-plugin feedback to settle. There
  is no external pressure to declare 1.0 early.

Everything else — the proxy method names, the signal set, the
lifecycle hook shape, the `metadata.json` keys — is shape-stable
from today. Changes in this window will be rare.
