# Corbomite Plugin Template

Minimum skeleton for a third-party Corbomite plugin.

## What's here

- `CMakeLists.txt` — build recipe using `corbomite_add_plugin()`.
- `metadata.json.in` — KPlugin + Corbomite metadata template.
- `TemplatePlugin.{h,cpp}` — stub `Plugin` subclass.
- `tests/` — smoke-test harness.

## Adapting

1. Copy this directory as the starting point for your plugin.
2. Rename `TemplatePlugin` to `YourPlugin` (filenames, class, factory macro).
3. Update `metadata.json.in`'s `KPlugin.Id` / `Name` / `Description`.
4. Declare the permissions your plugin needs in `X-Corbomite-Permissions`.
5. Build: `cmake -B build && cmake --build build`.
6. Package: see `docs/plugin-development/DISTRIBUTION.md`.

See `docs/plugin-development/TUTORIAL.md` for a full walkthrough with
the `note-stats` reference plugin.
