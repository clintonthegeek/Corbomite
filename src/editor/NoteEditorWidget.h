// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Markoff {
class Editor;
}

namespace Corbomite {

struct EphemeralState;
class NoteDocument;
class SourceEditor;
class VaultModel;
class VaultResourceProvider;
class CompletionPopup;
class HoverPopover;
class EditorSuggestManager;
class EditorSuggest;

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    // Three-mode encoding per Cluster E plan. `LivePreview` is Markoff's
    // cursor-in-block-reveals-source widget; `Source` is the plain-text
    // qutepart-corbomite widget; `Reading` is the (still-stub) read-only
    // view. On the wire these map through `ViewModeSerializer` to Obsidian's
    // compound `{mode, source}` shape — we do not persist the enum integer.
    enum class ViewMode { Source, LivePreview, Reading };
    Q_ENUM(ViewMode)

    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    Markoff::Editor *editor() const;

    // Optional — when set, hovers over wiki/markdown links schedule a 300ms
    // preview popover (Cluster H Phase 2). Lifetime owned by the caller.
    void setHoverPopover(HoverPopover *popover);

    // Cluster H Phase 3 — when set, cursor changes are dispatched to the
    // manager's registered EditorSuggest list (insertion-order first-wins).
    // Lifetime owned by the caller (typically MainWindow).
    void setEditorSuggestManager(EditorSuggestManager *manager);

    int currentLine() const;
    int currentColumn() const;

    // Cluster E Phase 1 — ephemeral-state round-trip. Captures / restores
    // scroll, cursor, mode, and fold through `Corbomite::EphemeralState`.
    // Not yet wired into production save/load paths (that is Phase 7); the
    // shape exists so tests + future wiring have a stable surface.
    EphemeralState saveEphemeralState() const;
    void restoreEphemeralState(const EphemeralState &state);

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);
    void viewModeChanged(ViewMode mode);

private:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onTextChanged();
    void onCursorPositionChanged(int line, int column);
    void syncFromDocument();

    // Completion via EditorSuggestManager (Cluster H Phase 3).
    void maybeActivateSuggester();
    void dismissCompletion();
    void onCompletionAccepted(const QString &text, const QString &data);
    void positionCompletionPopup();
    void updateCompletionFilter();
    int absoluteCursorPos() const;
    QString currentLineText() const;

    // Link resolution
    QString resolveTarget(const QString &target) const;

    Markoff::Editor *m_editor = nullptr;
    // Phase 2 mount of `Corbomite::SourceEditor` — constructed hidden so the
    // widget instantiation + library link is proven without changing any
    // user-visible behaviour. Cluster E Phase 7 will promote this into the
    // ViewMode switch (Editing / Reading / Source).
    SourceEditor *m_sourceEditor = nullptr;
    ViewMode m_viewMode = ViewMode::LivePreview;

    NoteDocument *m_doc = nullptr;
    VaultModel *m_vault = nullptr;
    VaultResourceProvider *m_resourceProvider = nullptr;
    bool m_updatingFromDoc = false;
    int m_cachedWordCount = 0;

    // Hover preview (lifetime owned by MainWindow).
    HoverPopover *m_hoverPopover = nullptr;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    EditorSuggestManager *m_suggestManager = nullptr;
    EditorSuggest *m_activeSuggester = nullptr;
    int m_completionTriggerPos = -1;
};

} // namespace Corbomite
