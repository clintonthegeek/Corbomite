// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qmarkdowntextedit.h>

namespace Corbomite {

class NoteDocument;
class VaultModel;
class CompletionPopup;

class NoteEditorWidget : public QMarkdownTextEdit {
    Q_OBJECT

public:
    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void onTextChanged();
    void onCursorPositionChanged();
    void syncFromDocument();

    // Completion
    void triggerWikiLinkCompletion();
    void triggerTagCompletion();
    void dismissCompletion();
    void onCompletionAccepted(const QString &text, const QString &data);
    void updateCompletionFilter();
    int completionTriggerPos() const;
    QString textFromTrigger() const;

    // Link navigation
    QString wikiLinkTargetAtCursor(const QPoint &pos) const;
    QString resolveWikiLinkTarget(const QString &rawTarget) const;

    NoteDocument *m_doc = nullptr;
    VaultModel *m_vault = nullptr;
    bool m_updatingFromDoc = false;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    int m_completionTriggerPos = -1;
    enum class CompletionMode { None, WikiLink, Tag };
    CompletionMode m_completionMode = CompletionMode::None;
    // Future: support [[Note|Display]] alias insertion mode
    // Future: respect "use markdown links" setting
};

} // namespace Corbomite
