// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTreeView>

namespace Corbomite {

class NotesTreeModel;
class MenuEventEmitter;

class FileExplorerPanel : public QWidget {
    Q_OBJECT

public:
    explicit FileExplorerPanel(QWidget *parent = nullptr);

    void setModel(NotesTreeModel *model);
    // Optional — when set, the right-click context menu emits Cluster H's
    // mid-construction `fileMenu` / `filesMenu` signal so plugins can push
    // items into named sections via MenuSectionHelper.
    void setMenuEventEmitter(MenuEventEmitter *emitter);

    QStringList expandedFolders() const;
    void restoreExpandedFolders(const QStringList &folders);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

Q_SIGNALS:
    void noteActivated(const QString &relativePath);
    void newNoteRequested(const QString &folderPath);
    void deleteNoteRequested(const QString &relativePath);
    void renameNoteRequested(const QString &relativePath);

private:
    void onDoubleClicked(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);

    QTreeView *m_treeView;
    MenuEventEmitter *m_menuEvents = nullptr;
};

} // namespace Corbomite
