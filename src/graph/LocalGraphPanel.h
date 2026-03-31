// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class NoteDocument;
class SQLiteIndex;
class VaultModel;

class LocalGraphPanel : public QWidget {
    Q_OBJECT

public:
    explicit LocalGraphPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setVaultModel(VaultModel *vault);
    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    void refresh();

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index = nullptr;
    VaultModel *m_vault = nullptr;
    NoteDocument *m_currentDoc = nullptr;
};

} // namespace Corbomite
