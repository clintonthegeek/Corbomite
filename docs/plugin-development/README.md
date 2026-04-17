# Corbomite Plugin Development

Documentation for authors of third-party Corbomite plugins.

## Table of contents

- [TUTORIAL.md](TUTORIAL.md) — end-to-end walkthrough of the note-stats
  reference plugin. Start here.
- [API-REFERENCE.md](API-REFERENCE.md) — every public proxy method,
  signal, permission token, lifecycle hook, and `metadata.json` key.
- [API-STABILITY.md](API-STABILITY.md) — what the stability contract
  covers, what it does not, and how we evolve the ABI. Read before
  committing to a plugin project.
- [DISTRIBUTION.md](DISTRIBUTION.md) — how to ship a plugin via distro
  package managers (`.deb`, `.rpm`, Arch PKGBUILD, nixpkgs) or via the
  user-local fallback path.

The reference plugin source lives at
[`examples/note-stats-plugin/`](../../examples/note-stats-plugin/);
the copy-to-start skeleton lives at
[`examples/plugin-template/`](../../examples/plugin-template/).

## Quickstart

```sh
# 1. Copy the template.
cp -r corbomite/examples/plugin-template my-plugin
cd my-plugin

# 2. Edit metadata.json.in (Id, Name, Permissions), rename the
#    TemplatePlugin class and files to match your plugin.

# 3. Build.
cmake -B build
cmake --build build

# 4. Install into the user-local plugin path so Corbomite discovers it.
cmake --install build --prefix ~/.local

# 5. Launch Corbomite, open Settings → Plugins, find your plugin, click
#    Enable. Approve the permission dialog on first enable.
```

For a walkthrough that explains each step (including the permission
model and the `createView` flow), follow
[TUTORIAL.md](TUTORIAL.md).

## Plugin model in one paragraph

A Corbomite plugin is a native C++ shared library (`.so`) built against
Qt 6, KDE Frameworks 6, and the Corbomite public API. It is loaded in-
process via `KPluginFactory`. The entry point is a subclass of
`Corbomite::Plugin` whose factory is registered with
`K_PLUGIN_FACTORY_WITH_JSON("metadata.json", ...)`. At enable time the
host reads the embedded `metadata.json`, displays its declared
permissions in a grant dialog (suppressed for built-ins — see
[API-STABILITY.md](API-STABILITY.md) §version compatibility), and then
calls `plugin->load(PluginContext *)`. The plugin pulls permission-gated
proxies from the context (`ctx->vault()`, `ctx->metadataCache()`,
`ctx->search()`, `ctx->commands()`, `ctx->views()`, etc.) and uses them
to register commands, register views, subscribe to vault signals, read
notes, write notes, store secrets, and spawn processes. Accessors for
permissions the plugin did not declare return `nullptr` — plugins are
expected to degrade gracefully. There is no sandbox: the permission
system is a declaration-of-intent honesty layer, not an enforcement
layer. Security is an ecosystem concern (distro review, code
inspection, reputation); the plugin runs as native code in the host
process and can do anything C++ code can do.

## Prerequisites

You should be comfortable with:

- C++20 + Qt 6 (signals, slots, `QObject`, `QWidget` if your plugin
  ships UI).
- A CMake build (the template ships one; third-party distribution
  needs you to adapt it slightly — see
  [DISTRIBUTION.md](DISTRIBUTION.md)).
- KDE Frameworks 6 basics (`KLocalizedString::i18n()`, `KConfigGroup`,
  `KPluginFactory`). If you have written a Kate, KDevelop, or Dolphin
  plugin before, the shape will be familiar.

You do *not* need to know anything about Obsidian. The Corbomite plugin
API is shape-compatible with Obsidian's `App.vault` / `App.fileManager`
/ `App.metadataCache` surfaces (by design — it makes porting easier),
but every concept is documented here in Corbomite's own terms.

## Getting help

- Open an issue against the Corbomite repository.
- The API is shape-stable from Cluster N close forward — see
  [API-STABILITY.md](API-STABILITY.md). If a method you need is missing,
  say so: additive changes to the proxy surface are always welcome.
