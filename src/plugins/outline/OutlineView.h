// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTimer>
#include <QTreeWidget>
#include <QWidget>

class QLabel;
class QTreeWidgetItem;

namespace Corbomite {

class MetadataCacheReader;
class VaultProxy;
class WorkspaceController;

class OutlineView : public QWidget
{
    Q_OBJECT
public:
    OutlineView(MetadataCacheReader *metadata,
                VaultProxy *vault,
                WorkspaceController *workspace,
                QWidget *parent = nullptr);

private Q_SLOTS:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onActiveFileChanged(const QString &path);

private:
    void scheduleRefresh();
    void refresh();

    MetadataCacheReader *m_metadata = nullptr;
    VaultProxy *m_vaultProxy = nullptr;
    WorkspaceController *m_workspace = nullptr;

    QLabel *m_headerLabel;
    QTreeWidget *m_tree;
    QLabel *m_emptyLabel;
    QTimer m_debounceTimer;

    QString m_currentPath;
};

} // namespace Corbomite
