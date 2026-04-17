// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QListWidget>
#include <QPointer>
#include <QWidget>

class QLabel;

namespace Corbomite {

class FileManagerProxy;
class MetadataCacheReader;
class VaultProxy;
class WorkspaceController;

class OutlinksView : public QWidget
{
    Q_OBJECT
public:
    OutlinksView(MetadataCacheReader *metadata,
                 VaultProxy *vault,
                 FileManagerProxy *fileManager,
                 WorkspaceController *workspace,
                 QWidget *parent = nullptr);

private Q_SLOTS:
    void onItemClicked(QListWidgetItem *item);
    void onActiveFileChanged(const QString &path);

private:
    void refresh();

    // Proxies are owned by PluginContext, which outlives this widget —
    // see PluginManager teardown ordering. Raw pointers; VaultProxy and
    // FileManagerProxy aren't QObjects, so QPointer doesn't apply.
    MetadataCacheReader *m_metadata = nullptr;
    VaultProxy *m_vaultProxy = nullptr;
    FileManagerProxy *m_fmProxy = nullptr;
    WorkspaceController *m_workspace = nullptr;

    QLabel *m_headerLabel;
    QListWidget *m_list;
    QLabel *m_emptyLabel;

    QString m_currentPath;
};

} // namespace Corbomite
