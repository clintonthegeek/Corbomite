#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build a portable Corbomite AppImage (x86_64).
#
# Usage (from repo root):
#   ./packaging/appimage/build-appimage.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TOOLS_DIR="${SCRIPT_DIR}/tools"
OUT_DIR="${SCRIPT_DIR}/out"
APPDIR="${SCRIPT_DIR}/AppDir"
BUILD_DIR="${REPO_ROOT}/build-appimage"

VERSION="$(sed -n 's/^project(Corbomite VERSION \([0-9.]*\).*/\1/p' "${REPO_ROOT}/CMakeLists.txt" | head -1)"
VERSION="${VERSION:-0.1.0}"
ARCH="$(uname -m)"
APPIMAGE_NAME="Corbomite-${VERSION}-${ARCH}.AppImage"

LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

fetch_tool() {
    local url="$1"
    local dest="$2"
    if [[ -x "${dest}" ]]; then
        return 0
    fi
    echo "==> Fetching $(basename "${dest}")"
    curl -fL --retry 3 -o "${dest}.partial" "${url}"
    mv "${dest}.partial" "${dest}"
    chmod +x "${dest}"
}

# Prefer extracted AppRun (works without FUSE; avoids re-running on real errors).
# Status messages go to stderr so command-substitution callers only capture the path.
ensure_extracted() {
    local tool="$1"
    local name extract_root
    name="$(basename "${tool}" .AppImage)"
    extract_root="${TOOLS_DIR}/${name}.extracted"
    if [[ ! -x "${extract_root}/squashfs-root/AppRun" ]]; then
        echo "==> Extracting ${name}" >&2
        rm -rf "${extract_root}"
        mkdir -p "${extract_root}"
        pushd "${extract_root}" >/dev/null
        "${tool}" --appimage-extract >/dev/null
        popd >/dev/null
    fi
    printf '%s\n' "${extract_root}/squashfs-root/AppRun"
}

run_appimage() {
    local tool="$1"
    shift
    local app
    app="$(ensure_extracted "${tool}")"
    "${app}" "$@"
}

echo "==> Corbomite AppImage ${VERSION} (${ARCH})"
mkdir -p "${TOOLS_DIR}" "${OUT_DIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}"

fetch_tool "${LINUXDEPLOY_URL}" "${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
fetch_tool "${LINUXDEPLOY_QT_URL}" "${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
fetch_tool "${APPIMAGETOOL_URL}" "${TOOLS_DIR}/appimagetool-x86_64.AppImage"

# linuxdeploy discovers plugins on PATH by basename.
export PATH="${TOOLS_DIR}:${PATH}"

cd "${REPO_ROOT}"

echo "==> Configure (preset appimage)"
cmake --preset appimage

echo "==> Build"
cmake --build --preset appimage -j"$(nproc)"

echo "==> Install into AppDir"
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

DESKTOP="${APPDIR}/usr/share/applications/com.concernednetizen.Corbomite.desktop"
ICON_SVG="${APPDIR}/usr/share/icons/hicolor/scalable/apps/com.concernednetizen.Corbomite.svg"
BIN="${APPDIR}/usr/bin/Corbomite"

[[ -x "${BIN}" ]] || { echo "error: missing ${BIN}" >&2; exit 1; }
[[ -f "${DESKTOP}" ]] || { echo "error: missing ${DESKTOP}" >&2; exit 1; }
[[ -f "${ICON_SVG}" ]] || { echo "error: missing ${ICON_SVG}" >&2; exit 1; }

cp -f "${ICON_SVG}" "${APPDIR}/com.concernednetizen.Corbomite.svg"
cp -f "${DESKTOP}" "${APPDIR}/com.concernednetizen.Corbomite.desktop"

# Private shared libs that live in the build tree (vault always installs;
# ryml/c4core may only appear under bin/).
mkdir -p "${APPDIR}/usr/lib"
for pattern in 'libvault.so*' 'libryml.so*' 'libc4core.so*'; do
    # shellcheck disable=SC2086
    for f in ${BUILD_DIR}/bin/${pattern} ${BUILD_DIR}/lib/${pattern}; do
        if [[ -e "${f}" ]]; then
            cp -a "${f}" "${APPDIR}/usr/lib/"
        fi
    done
done

# Drop static archives / CMake package files accidentally installed into lib/.
rm -f "${APPDIR}/usr/lib/"*.a
rm -rf "${APPDIR}/usr/lib/cmake"

# Plugins NEED libvault.so; expose it to linuxdeploy's dependency walker.
# Keep Corbomite plugins under usr/lib/plugins/corbomite (NOT qt6/plugins)
# so linuxdeploy-plugin-qt does not treat them as the Qt plugins root.
export LD_LIBRARY_PATH="${APPDIR}/usr/lib:${BUILD_DIR}/bin:${LD_LIBRARY_PATH:-}"

# Optional patchelf (system or tools/); rewrite plugin RPATH → usr/lib.
PATCHELF="$(command -v patchelf || true)"
if [[ -z "${PATCHELF}" && -x "${TOOLS_DIR}/patchelf" ]]; then
    PATCHELF="${TOOLS_DIR}/patchelf"
fi
if [[ -n "${PATCHELF}" ]]; then
    while IFS= read -r -d '' plug; do
        if [[ "${plug}" == *"/qt6/plugins/"* ]]; then
            "${PATCHELF}" --set-rpath '$ORIGIN/../../..' "${plug}"
        else
            "${PATCHELF}" --set-rpath '$ORIGIN/../..' "${plug}"
        fi
    done < <(find "${APPDIR}/usr/lib" -type f -name 'corbomite-*.so' -print0 2>/dev/null)
fi

echo "==> Seed essential Qt plugins (avoid pulling Firebird/GTK/Plasma stacks)"
QT_PLUGINS="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib/qt6/plugins)"
copy_plugins() {
    local subdir="$1"
    shift
    local src="${QT_PLUGINS}/${subdir}"
    local dst="${APPDIR}/usr/lib/qt6/plugins/${subdir}"
    [[ -d "${src}" ]] || return 0
    mkdir -p "${dst}"
    local f
    for f in "$@"; do
        if [[ -e "${src}/${f}" ]]; then
            cp -a "${src}/${f}" "${dst}/"
        fi
    done
}
copy_plugins platforms \
    libqxcb.so libqwayland.so libqoffscreen.so libqminimal.so
copy_plugins imageformats \
    libqgif.so libqico.so libqjpeg.so libqsvg.so
copy_plugins iconengines libqsvgicon.so
copy_plugins sqldrivers libqsqlite.so
copy_plugins tls libqcertonlybackend.so libqopensslbackend.so
copy_plugins xcbglintegrations \
    libqxcb-glx-integration.so libqxcb-egl-integration.so
# Wayland shell helpers (best-effort; skip if absent).
for sub in wayland-decoration-client wayland-graphics-integration-client \
           wayland-shell-integration platforminputcontexts; do
    if [[ -d "${QT_PLUGINS}/${sub}" ]]; then
        mkdir -p "${APPDIR}/usr/lib/qt6/plugins/${sub}"
        # Copy only .so files, not huge helper trees.
        find "${QT_PLUGINS}/${sub}" -maxdepth 1 -type f -name '*.so*' \
            -exec cp -a {} "${APPDIR}/usr/lib/qt6/plugins/${sub}/" \;
    fi
done

echo "==> Bundle shared library deps (linuxdeploy; no --plugin qt)"
# linuxdeploy-plugin-qt is unreliable on Qt 6.11 here; we seed plugins manually
# and let linuxdeploy pull NEEDED shared libs from the executable + plugins.
export QMAKE="/usr/bin/qmake6"
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QML2_IMPORT_PATH || true
run_appimage "${TOOLS_DIR}/linuxdeploy-x86_64.AppImage" \
    --appdir "${APPDIR}" \
    --executable "${BIN}" \
    --desktop-file "${APPDIR}/com.concernednetizen.Corbomite.desktop" \
    --icon-file "${APPDIR}/com.concernednetizen.Corbomite.svg"

# Wrap AppRun so plugin + private-lib paths resolve inside the AppDir.
if [[ -f "${APPDIR}/AppRun" ]]; then
    if [[ ! -f "${APPDIR}/AppRun.real" ]]; then
        mv "${APPDIR}/AppRun" "${APPDIR}/AppRun.real"
    fi
    cat > "${APPDIR}/AppRun" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/lib/plugins:${HERE}/usr/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/lib/plugins/platforms:${HERE}/usr/lib/qt6/plugins/platforms${QT_QPA_PLATFORM_PLUGIN_PATH:+:$QT_QPA_PLATFORM_PLUGIN_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
exec "${HERE}/AppRun.real" "$@"
EOF
    chmod +x "${APPDIR}/AppRun"
fi

echo "==> Package → ${OUT_DIR}/${APPIMAGE_NAME}"
rm -f "${OUT_DIR}/${APPIMAGE_NAME}"
ARCH="${ARCH}" run_appimage "${TOOLS_DIR}/appimagetool-x86_64.AppImage" \
    "${APPDIR}" "${OUT_DIR}/${APPIMAGE_NAME}"

chmod +x "${OUT_DIR}/${APPIMAGE_NAME}"
ls -lh "${OUT_DIR}/${APPIMAGE_NAME}"
echo "==> Done."
echo "    Smoke: ${OUT_DIR}/${APPIMAGE_NAME} --help"
