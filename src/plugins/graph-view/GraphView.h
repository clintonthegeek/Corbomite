// src/plugins/graph-view/GraphView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class GraphViewTab;
class SQLiteIndex;
class Vault;
class MetadataCache;
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

    void setIndex(SQLiteIndex *index);
    void setVault(Vault *vault);
    void setMetadataCache(MetadataCache *cache);
    void setControlsPanel(GraphControlsPanel *panel);
    GraphViewTab *graphWidget() const;

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

protected:
    void onOpen() override;

private:
    GraphViewTab *m_graphWidget = nullptr;
    SQLiteIndex *m_index = nullptr;
    Vault *m_vault = nullptr;
    MetadataCache *m_pendingCache = nullptr;
    GraphControlsPanel *m_pendingPanel = nullptr;
};

} // namespace Corbomite
