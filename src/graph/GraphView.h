// src/graph/GraphView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ItemView.h"

namespace Corbomite {

class GraphViewTab;
class SQLiteIndex;
class VaultModel;
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
    void setVaultModel(VaultModel *vault);
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
    VaultModel *m_vault = nullptr;
};

} // namespace Corbomite
