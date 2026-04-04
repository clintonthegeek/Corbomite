// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStackedWidget>
#include <QWidget>

namespace Markoff {
class Editor;
class ReadingView;
}

namespace Corbomite {

class NoteDocument;
class VaultModel;
class VaultResourceProvider;
class CompletionPopup;

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    enum class ViewMode { Source, LivePreview, Reading };
    Q_ENUM(ViewMode)

    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    Markoff::Editor *editor() const;

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);
    void viewModeChanged(ViewMode mode);

private:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onTextChanged();
    void onCursorPositionChanged(int line, int column);
    void syncFromDocument();

    // Completion
    void triggerWikiLinkCompletion();
    void triggerTagCompletion();
    void dismissCompletion();
    void onCompletionAccepted(const QString &text, const QString &data);

    // Link resolution
    QString resolveTarget(const QString &target) const;

    QStackedWidget *m_modeStack = nullptr;
    Markoff::Editor *m_editor = nullptr;
    Markoff::ReadingView *m_readingView = nullptr;
    ViewMode m_viewMode = ViewMode::Source;

    NoteDocument *m_doc = nullptr;
    VaultModel *m_vault = nullptr;
    VaultResourceProvider *m_resourceProvider = nullptr;
    bool m_updatingFromDoc = false;
    int m_cachedWordCount = 0;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    int m_completionTriggerPos = -1;
    enum class CompletionMode { None, WikiLink, Tag };
    CompletionMode m_completionMode = CompletionMode::None;
};

} // namespace Corbomite
