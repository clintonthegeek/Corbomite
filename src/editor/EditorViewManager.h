// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVector>
#include <memory>
#include <functional>
#include "NoteEditorWidget.h"
#include "corbomite/core/PaneLayout.h"

namespace Corbomite {

class MarkdownRenderEngine;
class NoteDocument;
class EditorViewSpace;
class SQLiteIndex;
class VaultModel;

class EditorViewManager : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewManager(QWidget *parent = nullptr);
    ~EditorViewManager() override;

    void openNote(NoteDocument *doc);
    void openCanvas(const QString &filePath);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;

    void setViewMode(NoteEditorWidget::ViewMode mode);
    NoteEditorWidget::ViewMode viewMode() const;

    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

    // New: split pane management
    void splitActiveHorizontal();
    void splitActiveVertical();
    void closeActiveViewSpace();
    int viewSpaceCount() const;

    bool queryClose();
    void closeAllDocuments();

    // --- PaneLayout (Obsidian workspace.json shape) ---

    /// Build a PaneLayout from the current splitter + tab state.
    /// `activeLeafId` in the result reflects the currently-focused tab.
    PaneLayout buildPaneLayout() const;

    /// Rebuild the splitter + pane widgets from a PaneLayout. `openTab`
    /// is called for each PaneLeaf with the owning EditorViewSpace and
    /// leaf data (file-path, view-type, mode, …). Caller realises the
    /// file-open inside that space.
    void applyPaneLayout(
        const PaneLayout &layout,
        std::function<void(EditorViewSpace *space, const PaneLeaf &leaf)> openTab);

    QVector<EditorViewSpace *> viewSpaces() const;
    QSplitter *rootSplitter() const;

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void graphNoteActivated(const QString &relativePath);

private:
    void setActiveViewSpace(EditorViewSpace *space);
    EditorViewSpace *createViewSpace();
    void connectViewSpace(EditorViewSpace *space);
    void splitActiveView(Qt::Orientation orientation);
    void removeViewSpace(EditorViewSpace *space);
    void cleanupEmptySplitters(QSplitter *splitter);
    void resetToSingleViewSpace();

    QSplitter *m_rootSplitter;
    EditorViewSpace *m_activeViewSpace = nullptr;
    QVector<EditorViewSpace *> m_viewSpaces;
    std::unique_ptr<MarkdownRenderEngine> m_canvasEngine;
};

} // namespace Corbomite
