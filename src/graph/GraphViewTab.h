// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <forcegraph/GraphTypes.h>

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class GraphControlsPanel;
class SQLiteIndex;
class VaultModel;

class GraphViewTab : public QWidget {
    Q_OBJECT

public:
    explicit GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent = nullptr);
    ~GraphViewTab() override;

    void buildGraph();
    void setControlsPanel(GraphControlsPanel *panel);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);
    void openNoteInNewTabRequested(const QString &relativePath);
    void revealInNavigationRequested(const QString &relativePath);
    void deleteNoteRequested(const QString &relativePath);

private:
    void wireControlsPanel();
    void applyFilters();
    void showNodeContextMenu(const QString &nodeId, const QPoint &globalPos);

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index;
    VaultModel *m_vault;

    GraphControlsPanel *m_controlsPanel = nullptr;

    // Cached full graph data (before filtering)
    QVector<ForceGraph::GraphNode> m_allNodes;
    QVector<ForceGraph::GraphEdge> m_allEdges;
};

} // namespace Corbomite
