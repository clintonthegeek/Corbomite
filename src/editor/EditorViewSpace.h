// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QHash>
#include "corbomite/models/TabModel.h"

namespace Corbomite {

class NoteDocument;
class NoteEditorWidget;

class EditorViewSpace : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewSpace(QWidget *parent = nullptr);

    void openNote(NoteDocument *doc);
    void closeTab(int index);
    NoteEditorWidget *activeEditor() const;
    TabModel *tabModel();

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);

private:
    void onTabChanged(int index);
    void onTabCloseRequested(int index);

    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    TabModel m_tabModel;
    QHash<QString, NoteEditorWidget *> m_editors; // relativePath -> editor
};

} // namespace Corbomite
