// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

class QMenu;

namespace Corbomite {

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
    void emitFileMenu(QMenu *menu, const QString &filePath);
    void emitUrlMenu(QMenu *menu, const QString &url);
    void emitEditorMenu(QMenu *menu, QObject *editor);
    void emitFilesMenu(QMenu *menu, const QStringList &filePaths);
    void emitLeafMenu(QMenu *menu, QObject *leaf);
    void emitTabGroupMenu(QMenu *menu, QObject *tabGroup);
    void emitMarkdownViewportMenu(QMenu *menu, QObject *viewport);

Q_SIGNALS:
    // Names mirror Obsidian Workspace events (verbatim camelCase).
    void fileMenu(QMenu *menu, const QString &filePath);
    void urlMenu(QMenu *menu, const QString &url);
    void editorMenu(QMenu *menu, QObject *editor);
    void filesMenu(QMenu *menu, const QStringList &filePaths);
    void leafMenu(QMenu *menu, QObject *leaf);
    void tabGroupMenu(QMenu *menu, QObject *tabGroup);
    void markdownViewportMenu(QMenu *menu, QObject *viewport);
};

} // namespace Corbomite
