// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

class QMenu;

namespace Corbomite {

// Source-discriminator constants for `MenuEventEmitter::fileMenu`. Mirrors
// the Obsidian `file-menu` event's `source` argument; plugin handlers use
// these to scope their reactions (e.g. only inject on tab-header right-
// click). Per `obsidian-audit/domains/workspace.md` §"file-menu":
//   "source ∈ \"file-explorer-context-menu\", \"link-context-menu\",
//    \"more-options\", \"pane-more-options\", \"sidebar-context-menu\",
//    \"tab-header\""
namespace FileMenuSource {
inline constexpr auto FileExplorerContextMenu = "file-explorer-context-menu";
inline constexpr auto LinkContextMenu = "link-context-menu";
inline constexpr auto MoreOptions = "more-options";
inline constexpr auto PaneMoreOptions = "pane-more-options";
inline constexpr auto SidebarContextMenu = "sidebar-context-menu";
inline constexpr auto TabHeader = "tab-header";
} // namespace FileMenuSource

// Mid-construction menu signal pattern from
// `docs/obsidian-audit/domains/workspace.md §4`.
//
// Construction order: caller adds core items → emits the appropriate signal
// → plugins (when they exist) push items into named sections via
// MenuSectionHelper → menu is shown.
//
// Today no plugins consume these signals — they go unobserved — but every
// menu-construction site in Corbomite emits anyway, so the substrate is
// ready when Cluster N's plugin layer lands.
class MenuEventEmitter : public QObject {
    Q_OBJECT

public:
    explicit MenuEventEmitter(QObject *parent = nullptr);

    // Convenience callers — emits the matching signal.
    //
    // `source` for `emitFileMenu` is a discriminator string (see
    // `FileMenuSource` constants above) that lets plugin handlers scope
    // their reactions to a specific invocation site. `leaf` is an optional
    // pointer to the WorkspaceLeaf the menu was opened from (passed as
    // QObject* to keep this header free of WorkspaceLeaf.h); plugins receive
    // the opaque leaf id from MenuInjector — raw pointers are not exposed
    // across the plugin surface.
    void emitFileMenu(QMenu *menu, const QString &filePath,
                       const QString &source, QObject *leaf = nullptr);
    void emitUrlMenu(QMenu *menu, const QString &url);
    void emitEditorMenu(QMenu *menu, QObject *editor);
    void emitFilesMenu(QMenu *menu, const QStringList &filePaths);
    void emitLeafMenu(QMenu *menu, QObject *leaf);
    void emitTabGroupMenu(QMenu *menu, QObject *tabGroup);
    void emitMarkdownViewportMenu(QMenu *menu, QObject *viewport);

Q_SIGNALS:
    // Names mirror Obsidian Workspace events (verbatim camelCase). The
    // `fileMenu` signature mirrors Obsidian's `(menu, file, source, leaf?)`
    // payload exactly.
    void fileMenu(QMenu *menu, const QString &filePath,
                   const QString &source, QObject *leaf);
    void urlMenu(QMenu *menu, const QString &url);
    void editorMenu(QMenu *menu, QObject *editor);
    void filesMenu(QMenu *menu, const QStringList &filePaths);
    void leafMenu(QMenu *menu, QObject *leaf);
    void tabGroupMenu(QMenu *menu, QObject *tabGroup);
    void markdownViewportMenu(QMenu *menu, QObject *viewport);
};

} // namespace Corbomite
