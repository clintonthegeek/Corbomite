// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class SQLiteIndex;
class VaultModel;

class GraphViewTab : public QWidget {
    Q_OBJECT

public:
    explicit GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent = nullptr);
    ~GraphViewTab() override;

    void buildGraph();

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index;
    VaultModel *m_vault;
    // Future: add filter controls panel overlay
};

} // namespace Corbomite
