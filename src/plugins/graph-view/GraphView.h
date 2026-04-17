// src/plugins/graph-view/GraphView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class GraphViewTab;
class SearchProxy;
class VaultProxy;
class MetadataCacheReader;
class GraphControlsPanel;

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
};

} // namespace Corbomite
