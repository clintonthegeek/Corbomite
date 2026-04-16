// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QHash>
#include <QVector>
#include "NoteEditorWidget.h"
#include "corbomite/models/TabModel.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/FileView.h"

namespace Corbomite {

class MarkdownRenderEngine;

class NoteDocument;
class GraphViewTab;
class CanvasViewTab;
class SQLiteIndex;
class VaultModel;
class HoverPopover;
class EditorSuggestManager;

class EditorViewSpace : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewSpace(QWidget *parent = nullptr);

    void setCanvasEngine(MarkdownRenderEngine *engine);
    // Cluster H Phase 2 — propagated to every NoteEditorWidget this space
    // owns (current and future) so hover-link previews work regardless of
    // when the tab opened.
    void setHoverPopover(HoverPopover *popover);
    // Cluster H Phase 3 — passed to every NoteEditorWidget (current + future)
    // for shared in-editor suggester dispatch.
    void setEditorSuggestManager(EditorSuggestManager *manager);
    void openNote(NoteDocument *doc);
    void closeTab(int index);
    NoteEditorWidget *activeEditor() const;
    TabModel *tabModel();

    void setViewMode(NoteEditorWidget::ViewMode mode);
    NoteEditorWidget::ViewMode viewMode() const;

    void openCanvas(const QString &filePath);
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

    // --- ViewRegistry-based API (Task 10) ---
    void setViewRegistry(ViewRegistry *registry);
    WorkspaceLeaf *openFile(const QString &relativePath);
    WorkspaceLeaf *openView(const QString &type, const QJsonObject &state = {});
    WorkspaceLeaf *activeLeaf() const;
    WorkspaceLeaf *leafForPath(const QString &relativePath) const;
    QVector<WorkspaceLeaf *> leaves() const;

    void closeAllTabs();
    bool hasModifiedDocuments() const;
    QStringList modifiedDocumentPaths() const;
    void saveAllModified();

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void internalLinkClicked(const QString &targetPath);
    void graphNoteActivated(const QString &relativePath);
    void splitRightRequested();
    void splitDownRequested();

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
    MarkdownRenderEngine *m_canvasEngine = nullptr;
    HoverPopover *m_hoverPopover = nullptr;
    EditorSuggestManager *m_suggestManager = nullptr;

    // ViewRegistry-based state (Task 10)
    ViewRegistry *m_viewRegistry = nullptr;
    QVector<WorkspaceLeaf *> m_leaves;
};

} // namespace Corbomite
