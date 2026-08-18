# Packaging Corbomite 0.1.0

Two first-cut formats for the initial release:

| Format | Path | Audience |
|---|---|---|
| **AppImage** | `packaging/appimage/build-appimage.sh` | Portable Linux binary |
| **Arch PKGBUILD** | `packaging/arch/PKGBUILD` | Arch / Manjaro (system deps) |

Both use a **Release** build with `CORBOMITE_DEV_BUILD=OFF` and Canvas-only LivePreview (`MARKOFF_BUILD_LIVE=OFF`, set by Corbomite's CMake). Desktop / D-Bus identity is `com.concernednetizen.Corbomite`.

## AppImage

```bash
# From the repository root:
./packaging/appimage/build-appimage.sh
```

The script will:

1. Download `linuxdeploy`, `linuxdeploy-plugin-qt`, and `appimagetool` into `packaging/appimage/tools/` (cached across runs).
2. Configure/build the `appimage` CMake preset (`build-appimage/`).
3. Stage into `packaging/appimage/AppDir` and emit  
   `packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage`.

Smoke:

```bash
./packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage --help
# or open a vault:
./packaging/appimage/out/Corbomite-0.1.0-x86_64.AppImage /path/to/vault
```

**Note:** KDDockWidgets still links Qt Declarative, so the AppImage will include some Qt Quick libraries even though Corbomite no longer uses the QML Live leaf.

## Arch package

```bash
cd packaging/arch
makepkg -f          # builds from the enclosing git checkout
# optional:
makepkg --printsrcinfo > .SRCINFO
sudo pacman -U corbomite-0.1.0-1-x86_64.pkg.tar.zst
```

Requires the usual Corbomite deps (`kddockwidgets`, KF6, Qt6, …) — see the `depends` / `makedepends` arrays in the PKGBUILD, and the README Building section.

For a published AUR package, switch `source=` to the Codeberg tag URL and bump `pkgrel` as needed.

## CMake presets

| Preset | Prefix | Notes |
|---|---|---|
| `dev` | (build tree) | Debug, `[Dev]` identity |
| `release` | `/usr/local` | Local `cmake --install` |
| `appimage` | `/usr` (DESTDIR=AppDir) | RPATH `$ORIGIN/../lib` |
