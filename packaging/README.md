# Packaging Corbomite 0.1.0

| Format | Path | Audience |
|---|---|---|
| **AppImage** | `packaging/appimage/build-appimage.sh` | Portable Linux binary (any modern x86_64 distro) |
| **Ubuntu .deb** | `packaging/ubuntu/build-deb.sh` (+ GitHub Actions) | Ubuntu **25.10+** (system Qt ≥ 6.8, KF6) |
| **Arch PKGBUILD** | `packaging/arch/PKGBUILD` | Arch / Manjaro (system deps) |

All use a **Release** build with `CORBOMITE_DEV_BUILD=OFF` and Canvas-only LivePreview (`MARKOFF_BUILD_LIVE=OFF`, set by Corbomite's CMake). Desktop / D-Bus identity is `com.concernednetizen.Corbomite`.

> **Why not Ubuntu 24.04 LTS?** Corbomite requires Qt **6.8+**. Ubuntu 24.04 ships Qt 6.4, so a native system `.deb` cannot target 24.04. Use the **AppImage** on 24.04 LTS; use the `.deb` on Ubuntu 25.10+.

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

## Ubuntu .deb (25.10+)

GitHub Actions builds this on every `v*` tag (`.github/workflows/release.yml`) inside an `ubuntu:25.10` container. KDDockWidgets is built from source and **bundled** into the package (not yet in Ubuntu 25.10).

Locally (requires Docker):

```bash
docker run --rm -u root \
  -e VERSION=0.1.0 \
  -v "$PWD":/workspace -w /workspace \
  ubuntu:25.10 \
  bash packaging/ubuntu/build-deb.sh
# → packaging/ubuntu/out/corbomite_0.1.0-1_amd64.deb
```

Install:

```bash
sudo apt install ./corbomite_0.1.0-1_amd64.deb
```

## Arch package

```bash
cd packaging/arch
makepkg -f          # builds from the enclosing git checkout
# optional:
makepkg --printsrcinfo > .SRCINFO
sudo pacman -U corbomite-0.1.0-1-x86_64.pkg.tar.zst
```

Requires the usual Corbomite deps (`kddockwidgets`, KF6, Qt6, …) — see the `depends` / `makedepends` arrays in the PKGBUILD, and the README Building section.

For a published AUR package, switch `source=` to a GitHub tag URL and bump `pkgrel` as needed.

## CMake presets

| Preset | Prefix | Notes |
|---|---|---|
| `dev` | (build tree) | Debug, `[Dev]` identity |
| `release` | `/usr/local` | Local `cmake --install` |
| `appimage` | `/usr` (DESTDIR=AppDir) | RPATH `$ORIGIN/../lib` |
