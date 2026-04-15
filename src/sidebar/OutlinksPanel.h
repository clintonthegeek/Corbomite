// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPointer>
#include <QLabel>

namespace Corbomite {

class NoteDocument;
class SQLiteIndex;
class VaultModel;
class MetadataCache;

class OutlinksPanel : public QWidget {
    Q_OBJECT

public:
    explicit OutlinksPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setMetadataCache(MetadataCache *cache);
    void setVaultModel(VaultModel *vault);
    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);
    void createNoteRequested(const QString &name);

private:
    void refresh();
    void onItemClicked(QListWidgetItem *item);

    QLabel *m_headerLabel;
    QListWidget *m_list;
    QLabel *m_emptyLabel;

    SQLiteIndex *m_index = nullptr;
    QPointer<MetadataCache> m_cache;
    VaultModel *m_vault = nullptr;
    QPointer<NoteDocument> m_currentDoc;
};

} // namespace Corbomite
