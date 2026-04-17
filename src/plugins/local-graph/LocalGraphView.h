// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace ForceGraph {
class ForceGraphView;
class ForceLayoutEngine;
}

namespace Corbomite {

class MetadataCacheReader;
class SearchProxy;
class VaultProxy;
class WorkspaceController;

class LocalGraphView : public QWidget
{
    Q_OBJECT
public:
    LocalGraphView(SearchProxy *search,
                   VaultProxy *vault,
                   MetadataCacheReader *metadata,
                   WorkspaceController *workspace,
                   QWidget *parent = nullptr);

private Q_SLOTS:
    void onActiveFileChanged(const QString &path);

private:
    void refresh();

    SearchProxy *m_search = nullptr;
    VaultProxy *m_vault = nullptr;
    MetadataCacheReader *m_metadata = nullptr;
    WorkspaceController *m_workspace = nullptr;

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    QString m_currentPath;
};

} // namespace Corbomite
