// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTreeView>

namespace Corbomite {

class NotesTreeModel;

class FileExplorerPanel : public QWidget {
    Q_OBJECT

public:
    explicit FileExplorerPanel(QWidget *parent = nullptr);

    void setModel(NotesTreeModel *model);

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
};

} // namespace Corbomite
