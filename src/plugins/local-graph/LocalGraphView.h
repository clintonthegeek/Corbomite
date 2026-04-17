// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace ForceGraph {
class ForceGraphView;
class ForceLayoutEngine;
}

namespace Corbomite {

class MetadataCacheReader;
class SQLiteIndex;
class Vault;
class WorkspaceController;

class LocalGraphView : public QWidget
{
    Q_OBJECT
public:
    LocalGraphView(SQLiteIndex *index,
                   Vault *vault,
                   MetadataCacheReader *metadata,
                   WorkspaceController *workspace,
                   QWidget *parent = nullptr);

private Q_SLOTS:
    void onActiveFileChanged(const QString &path);

private:
    void refresh();

    SQLiteIndex *m_index = nullptr;
    Vault *m_vault = nullptr;
    MetadataCacheReader *m_metadata = nullptr;
    WorkspaceController *m_workspace = nullptr;

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    QString m_currentPath;
};

} // namespace Corbomite
