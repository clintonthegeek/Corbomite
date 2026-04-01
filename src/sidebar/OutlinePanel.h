// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QTimer>
#include <QPointer>

namespace Corbomite {

class NoteDocument;

class OutlinePanel : public QWidget {
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void setCurrentNote(NoteDocument *doc);

Q_SIGNALS:
    void scrollToLine(int lineNumber);

private:
    void refresh();
    void onItemClicked(QTreeWidgetItem *item, int column);

    QLabel *m_headerLabel;
    QTreeWidget *m_tree;
    QLabel *m_emptyLabel;
    QTimer m_debounceTimer;

    QPointer<NoteDocument> m_currentDoc;
};

} // namespace Corbomite
