#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build a native Corbomite .deb for Ubuntu 25.10+ (Qt ≥ 6.8, KF6).
# Intended to run inside an ubuntu:25.10 container (see GitHub Actions).
#
# Usage (from repo root, as root inside the container):
#   bash packaging/ubuntu/build-deb.sh
#
# Environment:
#   VERSION          — package upstream version (default: from CMakeLists.txt)
#   KDDOCK_VERSION   — KDDockWidgets tag to build (default: v2.4.1)
#   OUT_DIR          — where to write the .deb (default: packaging/ubuntu/out)
#
set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
    echo "error: run as root inside an Ubuntu 25.10+ container" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUT_DIR="${OUT_DIR:-${SCRIPT_DIR}/out}"
BUILD_ROOT="${BUILD_ROOT:-/tmp/corbomite-deb-build}"
STAGE="${BUILD_ROOT}/stage"
KDDOCK_SRC="${BUILD_ROOT}/KDDockWidgets"
KDDOCK_VERSION="${KDDOCK_VERSION:-v2.4.1}"

VERSION="${VERSION:-$(sed -n 's/^project(Corbomite VERSION \([0-9.]*\).*/\1/p' "${REPO_ROOT}/CMakeLists.txt" | head -1)}"
VERSION="${VERSION:-0.1.0}"
PKG_VERSION="${PKG_VERSION:-${VERSION}-1}"
ARCH="$(dpkg --print-architecture)"
DEB_NAME="corbomite_${PKG_VERSION}_${ARCH}.deb"

export DEBIAN_FRONTEND=noninteractive
export LANG=C.UTF-8
export LC_ALL=C.UTF-8

echo "==> Corbomite Ubuntu .deb ${PKG_VERSION} (${ARCH})"
echo "    repo: ${REPO_ROOT}"

echo "==> apt update + build dependencies"
# Ensure universe is available (tree-sitter, some KF6 bits).
apt-get update -qq
apt-get install -y -qq --no-install-recommends software-properties-common \
    || true
if command -v add-apt-repository >/dev/null 2>&1; then
    add-apt-repository -y universe >/dev/null 2>&1 || true
    apt-get update -qq
fi
apt-get install -y -qq --no-install-recommends \
    ca-certificates \
    curl \
    git \
    cmake \
    ninja-build \
    pkg-config \
    build-essential \
    extra-cmake-modules \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-base-private-dev \
    qt6-svg-dev \
    qt6-declarative-dev \
    qt6-declarative-private-dev \
    qt6-tools-dev \
    qt6-wayland \
    libqt6sql6-sqlite \
    libcups2-dev \
    libgl-dev \
    libglx-dev \
    libopengl-dev \
    libtree-sitter-dev \
    libkf6coreaddons-dev \
    libkf6i18n-dev \
    libkf6xmlgui-dev \
    libkf6widgetsaddons-dev \
    libkf6iconthemes-dev \
    libkf6config-dev \
    libkf6configwidgets-dev \
    libkf6colorscheme-dev \
    libkf6dbusaddons-dev \
    libkf6syntaxhighlighting-dev \
    libkf6breezeicons6 \
    dpkg-dev \
    fakeroot \
    file

# Optional SecretStorage backend (best-effort).
apt-get install -y -qq --no-install-recommends qtkeychain-qt6-dev \
    || echo "note: qtkeychain-qt6-dev unavailable; SecretStorage will use in-process fallback"

# Docker bind-mounts are owned by the host user; silence git's safe.directory check.
git config --global --add safe.directory "${REPO_ROOT}" || true
git config --global --add safe.directory '*' || true

rm -rf "${BUILD_ROOT}"
mkdir -p "${BUILD_ROOT}" "${OUT_DIR}" "${STAGE}"

echo "==> Fetch KDDockWidgets ${KDDOCK_VERSION}"
git clone --depth 1 --branch "${KDDOCK_VERSION}" \
    https://github.com/KDAB/KDDockWidgets.git "${KDDOCK_SRC}"

echo "==> Build + install KDDockWidgets → /usr/local"
# KDDockWidgets 2.4+: Qt6 is the default frontend; disable examples/tests/docs.
# Needs qt6-*-private-dev for Qt6::WidgetsPrivate / QuickPrivate.
cmake -S "${KDDOCK_SRC}" -B "${BUILD_ROOT}/build-kddw" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DKDDockWidgets_FRONTENDS='qtwidgets;qtquick' \
    -DKDDockWidgets_EXAMPLES=OFF \
    -DKDDockWidgets_TESTS=OFF \
    -DKDDockWidgets_DOCS=OFF \
    -Wno-dev
cmake --build "${BUILD_ROOT}/build-kddw" -j"$(nproc)"
cmake --install "${BUILD_ROOT}/build-kddw"
ldconfig

echo "==> Configure Corbomite"
cmake -S "${REPO_ROOT}" -B "${BUILD_ROOT}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCORBOMITE_DEV_BUILD=OFF \
    -DCORBOMITE_PORT_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_PREFIX_PATH=/usr/local \
    -Wno-dev

echo "==> Build Corbomite"
cmake --build "${BUILD_ROOT}/build" -j"$(nproc)"

echo "==> Stage install into ${STAGE}"
DESTDIR="${STAGE}" cmake --install "${BUILD_ROOT}/build"

# Bundle KDDockWidgets shared libs into the package (not in Ubuntu 25.10 yet).
mkdir -p "${STAGE}/usr/lib/${ARCH}-linux-gnu"
for f in /usr/local/lib/libkddockwidgets-qt6.so* \
         /usr/local/lib/${ARCH}-linux-gnu/libkddockwidgets-qt6.so*; do
    if [[ -e "${f}" ]]; then
        cp -a "${f}" "${STAGE}/usr/lib/${ARCH}-linux-gnu/"
    fi
done
# Also catch plain /usr/local/lib64 layouts.
if compgen -G "/usr/local/lib64/libkddockwidgets-qt6.so*" >/dev/null; then
    cp -a /usr/local/lib64/libkddockwidgets-qt6.so* \
        "${STAGE}/usr/lib/${ARCH}-linux-gnu/"
fi

BIN="${STAGE}/usr/bin/Corbomite"
[[ -x "${BIN}" ]] || { echo "error: missing ${BIN}" >&2; exit 1; }

echo "==> Write DEBIAN control metadata"
mkdir -p "${STAGE}/DEBIAN"
INSTALLED_SIZE="$(du -sk "${STAGE}" | awk '{print $1}')"
# Runtime Depends: curated for Ubuntu 25.10 (questing). KDDockWidgets is bundled.
cat > "${STAGE}/DEBIAN/control" <<EOF
Package: corbomite
Version: ${PKG_VERSION}
Architecture: ${ARCH}
Maintainer: Clinton Molyneux <clinton@concernednetizen.com>
Section: editors
Priority: optional
Homepage: https://github.com/clintonthegeek/Corbomite
Installed-Size: ${INSTALLED_SIZE}
Depends: libc6, libstdc++6, libgcc-s1, libgl1, libglib2.0-0t64, libx11-6, libxcb1, libxcb-cursor0, libfontconfig1, libfreetype6, libharfbuzz0b, libpng16-16t64, zlib1g, libzstd1, libdouble-conversion3, libpcre2-16-0, libicu76 | libicu74 | libicu72, libdbus-1-3, libssl3t64 | libssl3, libsqlite3-0, libtree-sitter0.22 | libtree-sitter0, libqt6core6t64 (>= 6.8), libqt6gui6 (>= 6.8), libqt6widgets6 (>= 6.8), libqt6dbus6 (>= 6.8), libqt6network6 (>= 6.8), libqt6sql6 (>= 6.8), libqt6sql6-sqlite, libqt6svg6 (>= 6.8), libqt6printsupport6 (>= 6.8), libqt6opengl6 (>= 6.8), libqt6qml6 (>= 6.8), libqt6quick6 (>= 6.8), qt6-qpa-plugins, qt6-wayland, libkf6coreaddons6, libkf6i18n6, libkf6xmlgui6, libkf6widgetsaddons6, libkf6iconthemes6, libkf6configcore6, libkf6configgui6, libkf6configwidgets6, libkf6colorscheme6, libkf6dbusaddons6, libkf6syntaxhighlighting6, libkf6breezeicons6, libkf6archive6, libkf6guiaddons6, libkf6itemviews6, libkf6globalaccel6, libkf6codecs6
Recommends: qt6-gtk-platformtheme, fonts-noto-color-emoji
Description: Native Obsidian-compatible knowledge base for Linux
 Corbomite is a GPLv3 Qt6/KF6 desktop app for Obsidian-compatible vaults:
 Markdown notes with wikilinks, backlinks, Bases, Canvas, graph view, and a
 docked multi-pane workspace.
 .
 This package targets Ubuntu 25.10+ (system Qt ≥ 6.8). KDDockWidgets is
 bundled. Alpha software — back up your vaults.
EOF

cat > "${STAGE}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "${STAGE}/DEBIAN/postinst"

echo "==> Build ${DEB_NAME}"
rm -f "${OUT_DIR}/${DEB_NAME}"
fakeroot dpkg-deb --build "${STAGE}" "${OUT_DIR}/${DEB_NAME}"
dpkg-deb -I "${OUT_DIR}/${DEB_NAME}"
ls -lh "${OUT_DIR}/${DEB_NAME}"
echo "==> Done: ${OUT_DIR}/${DEB_NAME}"
