// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QWidget>

class QStackedWidget;

namespace Markoff {
// TODO(port-foundation-exploration): Markoff::Editor was the old live editor
// class (now retired in favor of LiveListModelBinding + QML). Forward-decl
// kept as a stub to satisfy lingering field/parameter declarations until
// the Live-side port lands.
class Editor;
class MarkdownView;
class MermaidRenderer;
}

namespace Markoff::Source {
// renamed: SourceEditor → Editor (2026-05-20 port)
class Editor;
}

namespace Markoff::Reading {
// TODO(port-foundation-exploration): Reading retired; stub forward-decl
// pending Live-with-editing-disabled rewiring.
class ReadingView;
}

namespace Corbomite {

struct EphemeralState;
class NoteDocument;
class Vault;
class VaultResourceProvider;
class CompletionPopup;
class HoverPopover;
class EditorSuggestManager;
class EditorSuggest;

namespace Core {
class ThemeService;
}

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    // Three-mode encoding per Cluster E plan. `LivePreview` is Markoff's
    // cursor-in-block-reveals-source widget; `Source` is the plain-text
    // qutepart-corbomite widget; `Reading` is the ReadingView widget. On the
    // wire these map through `ViewModeSerializer` to Obsidian's compound
    // `{mode, source}` shape — we do not persist the enum integer.
    enum class ViewMode { Source, LivePreview, Reading };
    Q_ENUM(ViewMode)

    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVault(Vault *vault);

    /// Phase C3 mode transition: (1) snapshot ephemeral state from outgoing
    /// leaf, (2) detach outgoing leaf via setDocument(nullptr), (3) swap
    /// QStackedWidget index (lazy-constructing the incoming widget if first
    /// visit), (4) attach incoming leaf via setDocument(markoff()), (5) restore
    /// ephemeral state. Content never round-trips through leaves during swap.
    /// Emits `viewModeChanged` on every real change.
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    Markoff::Editor *editor() const;
    Markoff::Source::Editor *sourceEditor() const;
    Markoff::Reading::ReadingView *readingView() const;

    // Returns the active MarkdownView leaf (any of the three), or nullptr if
    // none has been constructed yet. Cluster R / C7 consumers (MarkdownView
    // hamburger menu + MainWindow find-replace dispatch) call this for unified
    // virtual dispatch (showFindBar / showReplaceBar / hideFindBar / etc.).
    Markoff::MarkdownView *activeLeaf() const;

    // Optional — when set, hovers over wiki/markdown links schedule a 300ms
    // preview popover (Cluster H Phase 2). Lifetime owned by the caller.
    void setHoverPopover(HoverPopover *popover);

    // Cluster H Phase 3 — when set, cursor changes are dispatched to the
    // manager's registered EditorSuggest list (insertion-order first-wins).
    // Lifetime owned by the caller (typically MainWindow).
    void setEditorSuggestManager(EditorSuggestManager *manager);

    // C2 — subscribe to theme changes. When set, this widget applies
    // ThemeService::currentTheme() to every constructed leaf and follows
    // future themeChanged emissions. Lifetime owned by the caller.
    void setThemeService(Core::ThemeService *service);

    // C4 Task 14 — inject the host Mermaid renderer into both the Live leaf
    // (eagerly constructed) and the Reading leaf (applied on lazy construction).
    // Lifetime owned by the caller (typically MainWindow). Passing nullptr
    // clears the renderer; both leaves fall back to DefaultMermaidRenderer.
    void setMermaidRenderer(Markoff::MermaidRenderer *renderer);

    int currentLine() const;
    int currentColumn() const;

    /// Move the cursor to `line` (1-based). Dispatches per active ViewMode:
    /// Source uses Qutepart's setCursorPosition; LivePreview uses Markoff's
    /// goToLine; Reading is a no-op (no cursor). Returns true if the mode
    /// could apply the change, false for Reading mode or out-of-range.
    bool goToLine(int line);

    // Cluster E Phase 1/7 — ephemeral-state round-trip. Captures / restores
    // scroll, cursor, mode, and fold through `Corbomite::EphemeralState`.
    // Phase 7 wires these through `EditorViewManager::{build,apply}PaneLayout`
    // so workspace.json round-trips per-leaf eState.
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

    // --- Mode-transition helpers ---
    int stackIndexFor(ViewMode mode) const;
    void ensureWidgetConstructed(ViewMode mode);

    // (activeLeaf is now a public accessor — declared above with editor()/
    // sourceEditor()/readingView(). Internal callers below also use it.)

    // C2 — apply current theme to every constructed leaf (Live/Source/Reading).
    void applyThemeToAllLeaves();

    // Captures ephemeral state (scroll/cursor/fold) from the current active
    // leaf as a QJsonObject, and packs it into the Corbomite EphemeralState
    // envelope (mode + sourceFlag). Called before detaching the leaf.
    EphemeralState captureEphemeralStateFor(ViewMode mode) const;
    // Restores ephemeral state to the target leaf widget.
    void restoreEphemeralStateFor(ViewMode mode, const EphemeralState &s);

    QStackedWidget *m_stack = nullptr;

    Markoff::Editor *m_editor = nullptr;
    // Source mode widget — lazy. Constructed on first `setViewMode(Source)`
    // and cached in the stack thereafter. Accessor returns nullptr until
    // first construction. See `ensureWidgetConstructed`.
    Markoff::Source::Editor *m_sourceEditor = nullptr;
    // Reading mode widget — lazy. Same pattern as `m_sourceEditor`.
    Markoff::Reading::ReadingView *m_readingView = nullptr;
    ViewMode m_viewMode = ViewMode::LivePreview;

    // Indices populated as widgets are constructed. -1 means "not mounted
    // in the stack yet".
    int m_sourceIndex = -1;
    int m_livePreviewIndex = -1;
    int m_readingIndex = -1;

    NoteDocument *m_doc = nullptr;
    Vault *m_vault = nullptr;
    VaultResourceProvider *m_resourceProvider = nullptr;
    int m_cachedWordCount = 0;

    // Hover preview (lifetime owned by MainWindow).
    HoverPopover *m_hoverPopover = nullptr;

    // C2 — theme service (lifetime owned by MainWindow).
    Core::ThemeService *m_themeService = nullptr;

    // C4 Task 14 — mermaid renderer (lifetime owned by MainWindow).
    Markoff::MermaidRenderer *m_mermaidRenderer = nullptr;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    EditorSuggestManager *m_suggestManager = nullptr;
    EditorSuggest *m_activeSuggester = nullptr;
    int m_completionTriggerPos = -1;
};

} // namespace Corbomite
