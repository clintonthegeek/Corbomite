// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite::Permissions {

inline constexpr auto kVaultRead    = "vault.read";
inline constexpr auto kVaultWrite   = "vault.write";
inline constexpr auto kVaultEvents  = "vault.events";
inline constexpr auto kMetadataRead = "metadata.read";
inline constexpr auto kWorkspace    = "workspace";
inline constexpr auto kUiCommands   = "ui.commands";
inline constexpr auto kUiViews      = "ui.views";
inline constexpr auto kUiMenus      = "ui.menus";
inline constexpr auto kNetwork      = "network";
inline constexpr auto kSecrets      = "secrets";
inline constexpr auto kProcess      = "process";
inline constexpr auto kConfig       = "config";

inline constexpr auto kUiRendering  = "ui.rendering";
inline constexpr auto kUiEditor     = "ui.editor";
inline constexpr auto kUiStatusbar  = "ui.statusbar";
inline constexpr auto kUiIcons      = "ui.icons";
inline constexpr auto kProtocol     = "protocol";

} // namespace Corbomite::Permissions
