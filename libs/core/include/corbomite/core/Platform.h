// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::Platform {

/// Opens `absolutePath` through the OS default application for that file
/// type. Returns `true` on success.
///
/// Preflight: a non-existent path returns `false` without invoking any
/// external process. Callers should surface a user-visible notice on
/// failure — this function never pops UI itself.
bool openWithDefaultApp(const QString &absolutePath);

/// Opens the OS file manager at `absolutePath`'s parent folder, with the
/// file highlighted/selected where the platform supports selection.
///
/// - macOS: `open -R <abs>` (Finder reveals + selects).
/// - Windows: `explorer /select,<native-abs>` (Explorer reveals + selects).
/// - Linux: DBus FileManager1 `ShowItems([uri], "corbomite")` first
///   (works on Dolphin / Nautilus / Nemo / PCManFM-Qt); falls back to
///   `QDesktopServices::openUrl` on the parent folder (no selection).
/// - Other: fall back to opening the parent folder.
///
/// Non-existent paths return `false`.
bool showInFolder(const QString &absolutePath);

}  // namespace Corbomite::Platform
