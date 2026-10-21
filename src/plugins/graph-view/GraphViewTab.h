// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QWidget>
#include <corbomite/forcegraph/GraphTypes.h>

namespace Corbomite::ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class GraphControlsPanel;
class SearchProxy;
class VaultProxy;
class MetadataCacheReader;

class GraphViewTab : public QWidget {
    Q_OBJECT

public:
    explicit GraphViewTab(SearchProxy *search, VaultProxy *vault, QWidget *parent = nullptr);
    ~GraphViewTab() override;

    void buildGraph();

    /// Each tab OWNS its controls panel.
    ///
    /// This used to be a singleton handed in from GraphViewPlugin and wired
    /// by every tab with ~13 connect() calls and no disconnect, so two open
    /// graph tabs both reacted to one panel's sliders. Owning
    /// it fixes that by construction and removes the need for a disconnect.
    ///
    /// The panel is created on first request so it is not built for a tab
    /// the user never opens the controls on; `ownsControlsPanel()` reports
    /// whether it exists yet.
    GraphControlsPanel *controlsPanel();
    bool ownsControlsPanel() const { return !m_controlsPanel.isNull(); }
    void setMetadataCache(MetadataCacheReader *cache);

    /// Access to the underlying ForceGraphView so GraphView can dispatch
    /// View::zoomIn/Out/Reset onto the real viewport transform.
    Corbomite::ForceGraph::ForceGraphView *graphView() const { return m_graphView; }

    // ---- the graph verbs, hoisted out of the
    // controls panel's clicked lambdas so GraphViewActions can drive them.
    //
    // Both bodies are unchanged; only their home moved. Same pattern as the
    // bases toolbar.
    void animate();          ///< re-randomize positions and restart layout
    void resetForces();      ///< reset the controls panel's sliders to defaults
    void zoomToFitGraph();   ///< fit the whole graph in the viewport
    bool isGraphBuilt() const;

    /// Test seam — the tab's own engine's forces. Exposed because the
    /// per-instance fix is only verifiable if each tab's engine state can be
    /// read independently, and the panel only exposes its own copy.
    double engineCenterForce() const;
    void setEngineCenterForce(double force);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);
    void openNoteInNewTabRequested(const QString &relativePath);
    void revealInNavigationRequested(const QString &relativePath);
    void deleteNoteRequested(const QString &relativePath);

private:
    void wireControlsPanel();
    /// Seed a freshly-created panel from the engine's current forces.
    void syncPanelFromEngine();
    void applyFilters();
    void showNodeContextMenu(const QString &nodeId, const QPoint &globalPos);

    Corbomite::ForceGraph::ForceGraphView *m_graphView;
    Corbomite::ForceGraph::ForceLayoutEngine *m_engine;
    SearchProxy *m_search;
    VaultProxy *m_vault;

    QPointer<GraphControlsPanel> m_controlsPanel;
    /// Wrapper widget so the panel can be inserted into the pane's layout
    /// without the panel itself being re-parented mid-construction.
    QWidget *m_panelHost = nullptr;
    QPointer<MetadataCacheReader> m_cache;

    // Cached full graph data (before filtering)
    QVector<Corbomite::ForceGraph::GraphNode> m_allNodes;
    QVector<Corbomite::ForceGraph::GraphEdge> m_allEdges;
};

} // namespace Corbomite
