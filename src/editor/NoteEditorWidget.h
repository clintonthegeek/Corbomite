// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qmarkdowntextedit.h>

namespace Corbomite {

class NoteDocument;

class NoteEditorWidget : public QMarkdownTextEdit {
    Q_OBJECT

public:
    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);

private:
    void onTextChanged();
    void onCursorPositionChanged();
    void syncFromDocument();

    NoteDocument *m_doc = nullptr;
    bool m_updatingFromDoc = false;
};

} // namespace Corbomite
