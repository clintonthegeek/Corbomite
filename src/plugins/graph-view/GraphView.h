// src/plugins/graph-view/GraphView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

#include <functional>

namespace Corbomite {

class GraphViewTab;
class SearchProxy;
class VaultProxy;
class MetadataCacheReader;
class GraphControlsPanel;
class MenuSectionHelper;

class GraphView : public ItemView
{
    Q_OBJECT
public:
    explicit GraphView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getDisplayText() const override;
    QString getIcon() const override;

    void setSearch(SearchProxy *search);
    void setVault(VaultProxy *vault);
    void setMetadataCache(MetadataCacheReader *cache);
    void setControlsPanel(GraphControlsPanel *panel);
    GraphViewTab *graphWidget() const;

    // Cluster O Phase O1.T3 — re-light View::zoom*() onto the real
    // ForceGraphView viewport transform (previously the graph had no zoom
    // action at all — MainWindow's zoom slots only ever reached the
    // markdown leaf).
    void zoomIn() override;
    void zoomOut() override;
    void zoomReset() override;

    // Cluster O Phase O2 — no View::canX()/contextChanged() overrides
    // needed here. canZoom() stays the base default (true — matches the
    // real zoom above); every other capability (canEdit/canSave/canFind/
    // hasSelection/canUndo/canRedo) is a correct "no" for graph today, and
    // graph has nothing that changes those answers mid-session, so it
    // never needs to emit contextChanged() either (constant capabilities).

    /// Cluster R Task 3.7 — command dispatcher injected by the graph
    /// plugin (captures CommandRegistrar from PluginContext). Wired so
    /// the hamburger Split / Copy-screenshot / Bookmark entries can
    /// reach the registry.
    using CommandDispatch = std::function<void(const QString &commandId)>;
    void setGraphCommandDispatcher(CommandDispatch dispatcher);

    /// Cluster R Task 3.7 — hamburger menu: Split / Copy screenshot +
    /// Bookmark placeholder. Does NOT chain to EditableFileView (GraphView
    /// is an ItemView — no backing file).
    void onMoreOptionsMenu(MenuSectionHelper &helper) override;

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

protected:
    void onOpen() override;

private:
    GraphViewTab *m_graphWidget = nullptr;
    SearchProxy *m_search = nullptr;
    VaultProxy *m_vault = nullptr;
    MetadataCacheReader *m_pendingCache = nullptr;
    GraphControlsPanel *m_pendingPanel = nullptr;
    CommandDispatch m_graphCommandDispatcher;
};

} // namespace Corbomite
