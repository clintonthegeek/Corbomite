// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Corbomite {

class NoteDocument;
class NoteEditorWidget;
class EditorViewSpace;
class SQLiteIndex;
class VaultModel;

class EditorViewManager : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewManager(QWidget *parent = nullptr);

    void openNote(NoteDocument *doc);
    void openCanvas(const QString &filePath);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;
    void toggleEditorMode();
    bool isPreviewMode() const;
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void graphNoteActivated(const QString &relativePath);

private:
    EditorViewSpace *m_viewSpace;
};

} // namespace Corbomite
