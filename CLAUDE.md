## Building

Configure and build with the dev build flag:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build
```

Run:
```bash
./build/Corbomite
```

Run tests:
```bash
cd build && ctest --output-on-failure
```

### Dev Build Isolation

Always configure with `-DCORBOMITE_DEV_BUILD=ON` so dev builds use isolated config/data directories and don't interfere with any installed release version.

| | Release | Dev (`-DCORBOMITE_DEV_BUILD=ON`) |
|---|---|---|
| Config | `~/.config/corbomiterc` | `~/.config/corbomite-devrc` |
| Data | `~/.local/share/corbomite/` | `~/.local/share/corbomite-dev/` |
| Window title | "Corbomite" | "Corbomite [Dev]" |

## Library Structure

| Library | Target | Purpose |
|---------|--------|---------|
| `libs/core` | `Corbomite::Core` | Domain types: NoteMeta, NoteDocument |
| `libs/storage` | `Corbomite::Storage` | File I/O: FileSystemAdapter, VaultScanner |
| `libs/models` | `Corbomite::Models` | Qt item models: VaultModel, NotesTreeModel, TabModel |
| `libs/qmarkdowntextedit` | `qmarkdowntextedit` | Markdown editor widget (git submodule) |

## Testing

Tests define **expected behavior**. When a test fails, fix the code, not the test.

Run a single test:
```bash
cd build && ctest -R tst_notemeta --output-on-failure
```

## Code Conventions

- C++20, Qt6/KDE Frameworks 6
- Use `i18n()` for all user-visible strings
- Use `QIcon::fromTheme()` for all icons
- Use `KStandardAction` where applicable
- GPLv3 license
