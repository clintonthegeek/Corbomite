// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVector>
#include <QJsonObject>
#include <memory>
#include <functional>

namespace Corbomite {

class MarkdownRenderEngine;
class NoteDocument;
class NoteEditorWidget;
class EditorViewSpace;
class SQLiteIndex;
class VaultModel;

class EditorViewManager : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewManager(QWidget *parent = nullptr);
    ~EditorViewManager() override;

    // Existing API — targets active view space
    void openNote(NoteDocument *doc);
    void openCanvas(const QString &filePath);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;
    void toggleEditorMode();
    bool isPreviewMode() const;
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

    // New: split pane management
    void splitActiveHorizontal();
    void splitActiveVertical();
    void closeActiveViewSpace();
    int viewSpaceCount() const;

    bool queryClose();
    void closeAllDocuments();

    // Session save/restore
    QJsonObject buildSessionState() const;
    void restoreFromSession(const QJsonObject &editorState,
                            std::function<void(const QString &path, EditorViewSpace *space)> openTabCallback);
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
    void rebuildSplitLayout(const QJsonValue &node, QSplitter *parent);
    void restoreTabState(const QJsonObject &paneJson, EditorViewSpace *space);

    QSplitter *m_rootSplitter;
    EditorViewSpace *m_activeViewSpace = nullptr;
    QVector<EditorViewSpace *> m_viewSpaces;
    std::unique_ptr<MarkdownRenderEngine> m_readingEngine;
    std::unique_ptr<MarkdownRenderEngine> m_canvasEngine;
};

} // namespace Corbomite
