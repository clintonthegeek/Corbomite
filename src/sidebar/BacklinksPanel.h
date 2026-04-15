// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPointer>
#include <QLabel>

namespace Corbomite {

class NoteDocument;
class SQLiteIndex;
class MetadataCache;

class BacklinksPanel : public QWidget {
    Q_OBJECT

public:
    explicit BacklinksPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void setMetadataCache(MetadataCache *cache);
    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    void refresh();
    void onItemClicked(QListWidgetItem *item);

    QLabel *m_headerLabel;
    QListWidget *m_list;
    QLabel *m_emptyLabel;

    SQLiteIndex *m_index = nullptr;
    QPointer<MetadataCache> m_cache;
    QPointer<NoteDocument> m_currentDoc;
};

} // namespace Corbomite
