# Distribution

How to ship a Corbomite plugin so users can install it.

## Canonical path: distro packages

Corbomite plugins are native shared libraries. The idiomatic way to
distribute them is through the operating system's package manager:

- `.deb` (Debian, Ubuntu, derivatives).
- `.rpm` (Fedora, openSUSE, RHEL, derivatives).
- Arch `PKGBUILD` (Arch, Manjaro).
- `nixpkgs` expression (NixOS, Nix).

This matches how KDE Frameworks themselves ship: users get code
signed by their distribution's key, automatic dependency resolution,
centralised update handling, and a review process appropriate to the
distro. Corbomite deliberately does **not** operate a plugin store,
an auto-updater, or a `curl | sh` install path. If you want an
installable-by-typical-user plugin, ship it through a distro.

The examples below are minimum recipes. Real packages add manifests,
changelogs, signing, and CI; those are out of scope here.

## File layout

A correctly-installed plugin consists of exactly two files, both in
the same directory:

```
<plugindir>/<your-plugin>.so
<plugindir>/metadata.json
```

`<plugindir>` is normally
`${KDE_INSTALL_PLUGINDIR}/corbomite/`, which resolves via
`KDEInstallDirs`:

- Debian/Ubuntu: `/usr/lib/x86_64-linux-gnu/qt6/plugins/corbomite/`
  (multi-arch triple varies).
- Fedora/openSUSE: `/usr/lib64/qt6/plugins/corbomite/`.
- Arch: `/usr/lib/qt6/plugins/corbomite/`.

`corbomite_add_plugin()` already emits the `install(TARGETS ...)`
rule for `${KDE_INSTALL_PLUGINDIR}/corbomite/` when
`KDEInstallDirs` is in the project, so you usually do not need to
hand-write install rules.

The `metadata.json` next to the `.so` is embedded into the `.so`
itself via `K_PLUGIN_FACTORY_WITH_JSON`, but many tools (including
the Corbomite `PluginsPage` listing before the library loads) read
it from disk. Ship both.

## Minimum Debian packaging

`debian/control`:

```
Source: corbomite-note-stats
Section: text
Priority: optional
Maintainer: Your Name <you@example.com>
Build-Depends: debhelper-compat (= 13),
               cmake,
               extra-cmake-modules,
               qt6-base-dev,
               libkf6coreaddons-dev,
               libkf6i18n-dev,
               corbomite-dev
Standards-Version: 4.6.2

Package: corbomite-note-stats
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}, corbomite (>= 0.1)
Description: Vault statistics sidebar for Corbomite
 Shows vault-wide note count, word count, tag count, and link count.
```

`debian/rules`:

```make
#!/usr/bin/make -f

%:
	dh $@ --buildsystem=cmake

override_dh_auto_configure:
	dh_auto_configure -- \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Build:

```sh
dpkg-buildpackage -us -uc
```

The `corbomite-dev` build-dep provides the headers + CMake package
config; the `corbomite (>= 0.1)` runtime dep matches your
`X-Corbomite-MinVersion`.

## Minimum Fedora/openSUSE RPM spec

`corbomite-note-stats.spec`:

```
Name:           corbomite-note-stats
Version:        0.1.0
Release:        1%{?dist}
Summary:        Vault statistics sidebar for Corbomite

License:        GPL-3.0-or-later
URL:            https://example.com/corbomite-note-stats
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  corbomite-devel >= 0.1.0

Requires:       corbomite >= 0.1.0

%description
Shows vault-wide note count, word count, tag count, and link count
in the right sidebar of Corbomite.

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%{_qt6_plugindir}/corbomite/note-stats.so
%{_qt6_plugindir}/corbomite/metadata.json
%license LICENSE

%changelog
* Thu Apr 17 2026 Your Name <you@example.com> - 0.1.0-1
- Initial package.
```

Build:

```sh
rpmbuild -ba corbomite-note-stats.spec
```

## Arch `PKGBUILD`

```bash
pkgname=corbomite-note-stats
pkgver=0.1.0
pkgrel=1
pkgdesc="Vault statistics sidebar for Corbomite"
arch=('x86_64')
url="https://example.com/corbomite-note-stats"
license=('GPL-3.0-or-later')
depends=('corbomite>=0.1.0' 'qt6-base' 'kcoreaddons' 'ki18n')
makedepends=('cmake' 'extra-cmake-modules' 'corbomite')
source=("$pkgname-$pkgver.tar.gz::https://example.com/releases/$pkgname-$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cmake -B build -S "$pkgname-$pkgver" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
```

Build: `makepkg -s`.

## nixpkgs

```nix
{ stdenv, cmake, extra-cmake-modules, qt6, kdePackages, corbomite }:

stdenv.mkDerivation rec {
  pname = "corbomite-note-stats";
  version = "0.1.0";
  src = ...;

  nativeBuildInputs = [ cmake extra-cmake-modules qt6.wrapQtAppsHook ];
  buildInputs = [
    qt6.qtbase
    kdePackages.kcoreaddons
    kdePackages.ki18n
    corbomite
  ];

  meta = with lib; {
    description = "Vault statistics sidebar for Corbomite";
    license = licenses.gpl3Plus;
    platforms = platforms.linux;
  };
}
```

## Distribution without a distro package

For single-user install (e.g. during plugin development, or for
internal-only plugins not ready for a distro submission), drop the
`.so` and its `metadata.json` into:

```
~/.local/share/qt6/plugins/corbomite/
```

or any of the Qt plugin discovery paths Corbomite appends at startup.
Corbomite discovers both system and user paths at vault-open time.

`cmake --install build --prefix ~/.local` from an out-of-tree build
handles this automatically when `KDEInstallDirs` is configured.

**Trust flag.** `X-Corbomite-Trusted: true` is controlled by the
Corbomite build system, not by install path. A user-local plugin is
always untrusted regardless of what its metadata declares
(`PluginManager` demotes any `Origin::User` plugin's trusted claim
to `false`). This means the first-enable permission dialog fires for
all third-party plugins, and the user clicks through it deliberately.

## Versioning alongside Corbomite

Set `X-Corbomite-MinVersion` in your plugin's `metadata.json` to the
Corbomite release you tested against:

```json
"X-Corbomite-MinVersion": "0.1.0"
```

`PluginManager::enablePlugin` compares this against
`QCoreApplication::applicationVersion()` and refuses to enable
plugins whose minimum exceeds the running host, surfacing "Requires
Corbomite >= X" in `PluginsPage`. This is the clean-refusal path: a
user who upgrades their plugin without upgrading Corbomite gets a
readable message, not a crash.

Mirror this in your distro package's runtime dependency — e.g.
`Depends: corbomite (>= 0.1.0)` in Debian, `Requires: corbomite >=
0.1.0` in RPM. The host refusal is the belt; the package dependency
is the suspenders.

Also consider `X-Corbomite-ApiLevel` (defaults to `1`). If you need
features that require a future plugin API level, bump the declared
level; Corbomite will refuse to load your plugin on hosts that do
not support that level. See [API-STABILITY.md](API-STABILITY.md)
§version compatibility.

## Distribution checklist

Before submitting to a distro repository:

- [ ] `metadata.json.in` has a unique namespaced `Id`
      (e.g. `yourname.thing`, not `thing`).
- [ ] `X-Corbomite-Permissions` lists every permission the plugin
      uses, and no more. Users will see this list in the grant
      dialog.
- [ ] `X-Corbomite-MinVersion` matches the Corbomite release you
      tested against.
- [ ] `X-Corbomite-ApiLevel` is `1` unless you depend on a future
      plugin API level.
- [ ] `KPlugin.License` is an SPDX identifier matching the source
      licence.
- [ ] A smoke test (like `examples/note-stats-plugin/tests/`) is
      included and runs under `ctest`.
- [ ] The package depends on `corbomite (>= $minVersion)` in the
      distro's dependency syntax.
- [ ] The source tarball builds cleanly in a clean chroot / mock /
      pbuilder.
