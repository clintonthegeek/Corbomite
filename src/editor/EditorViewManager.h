// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Corbomite {

class NoteDocument;
class NoteEditorWidget;
class EditorViewSpace;

class EditorViewManager : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewManager(QWidget *parent = nullptr);

    void openNote(NoteDocument *doc);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);

private:
    EditorViewSpace *m_viewSpace;
};

} // namespace Corbomite
