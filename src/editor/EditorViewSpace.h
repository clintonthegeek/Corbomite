// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QHash>
#include <QSet>
#include "corbomite/models/TabModel.h"

namespace Corbomite {

class NoteDocument;
class NoteEditorWidget;
class NotePreviewWidget;
class GraphViewTab;
class SQLiteIndex;
class VaultModel;

class EditorViewSpace : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewSpace(QWidget *parent = nullptr);

    void openNote(NoteDocument *doc);
    void closeTab(int index);
    NoteEditorWidget *activeEditor() const;
    TabModel *tabModel();
    void toggleEditorMode();
    bool isPreviewMode() const;
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void internalLinkClicked(const QString &targetPath);
    void graphNoteActivated(const QString &relativePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void showTabContextMenu(const QPoint &pos);

    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    TabModel m_tabModel;
    QHash<QString, NoteEditorWidget *> m_editors; // relativePath -> editor
    QHash<QString, NotePreviewWidget *> m_previews;
    QSet<QString> m_previewModePaths; // paths currently in preview mode
};

} // namespace Corbomite
