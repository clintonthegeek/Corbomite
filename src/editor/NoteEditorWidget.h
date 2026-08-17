// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QWidget>

#include <markoff/core/EditorContext.h>
#include <markoff/core/LinkActivation.h>

class QStackedWidget;

namespace Markoff {
class MarkdownView;
class MermaidRenderer;  // stub forward decl — type undefined post-port (E5 work)
class DefaultLinkService;
}

namespace Markoff::Live {
class EditorWidget;
}

namespace Markoff::Canvas {
// Cluster K — alternate LivePreview backend, behind a settings toggle
// (CorbomiteSettings::canvasLivePreview). See NoteEditorWidget.cpp ctor.
class EditorWidget;
}

namespace Markoff::Source {
// renamed: SourceEditor → Editor (2026-05-20 port)
class Editor;
}

namespace Markoff::Styled {
class Editor;
}

namespace Corbomite {

struct EphemeralState;
class NoteDocument;
class Vault;
class VaultResourceProvider;
class CompletionController;
class HoverPopover;
class EditorSuggestManager;
class FindBar;

namespace Core {
class ThemeService;
}

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    // Three-mode encoding per Cluster E plan. `LivePreview` is Markoff's
    // cursor-in-block-reveals-source widget; `Source` is the plain-text
    // editor widget; `Reading` is a read-only `Markoff::Styled::Editor` leaf
    // (QWidget, no QML). On the wire these map through `ViewModeSerializer` to
    // Obsidian's compound `{mode, source}` shape — we do not persist the enum
    // integer.
    enum class ViewMode { Source, LivePreview, Reading };
    Q_ENUM(ViewMode)

    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVault(Vault *vault);

    void showFindBar();
    void showReplaceBar();
    void hideFindBar();
    bool isFindBarVisible() const;

    /// Phase C3 mode transition: (1) snapshot ephemeral state from outgoing
    /// leaf, (2) detach outgoing leaf via setDocument(nullptr), (3) swap
    /// QStackedWidget index (lazy-constructing the incoming widget if first
    /// visit), (4) attach incoming leaf via setDocument(markoff()), (5) restore
    /// ephemeral state. Content never round-trips through leaves during swap.
    /// Emits `viewModeChanged` on every real change.
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    Markoff::Live::EditorWidget *editor() const;
    Markoff::Source::Editor *sourceEditor() const;

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

    /// Move the cursor to `line` (1-based flat visual line). Dispatches via
    /// the MarkdownView base; works in all three modes (Reading keeps a
    /// caret while read-only). Returns false only when no leaf exists yet.
    bool goToLine(int line);

    /// Insert `text` at the active leaf's caret as one undo-integrated D2 edit
    /// (propagates to every leaf), instead of appending at end-of-document.
    /// If `caretMarker` is non-empty and present in `text`, it is stripped and
    /// the caret lands at its position; otherwise the caret lands after the
    /// inserted text. Returns false when there is no document. Used by
    /// template insertion (road-to-dogfood Phase 2).
    bool insertAtCursor(const QString &text, const QString &caretMarker = {});

    // Cluster E Phase 1/7 — ephemeral-state round-trip. Captures / restores
    // scroll, cursor, mode, and fold through `Corbomite::EphemeralState`.
    // Phase 7 wires these through `EditorViewManager::{build,apply}PaneLayout`
    // so workspace.json round-trips per-leaf eState.
    EphemeralState saveEphemeralState() const;
    void restoreEphemeralState(const EphemeralState &state);

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    // openInNewTab mirrors Markoff::LinkActivation::openInNewTab (true for
    // an explicit middle-click; false for a plain click, which should
    // navigate the current leaf in place instead of always creating a new
    // one — see MainWindow's connection for the in-place-vs-new-tab split).
    void linkActivated(const QString &targetPath, bool openInNewTab);
    void viewModeChanged(ViewMode mode);
    // Contract v2: re-emitted from whichever leaf is active. Inactive leaves
    // are detached from the document and silent, but the leaf == activeLeaf()
    // guard in wireLeaf makes that explicit.
    void editorContextChanged(const Markoff::EditorContext &ctx);

private:
    void onCursorPositionChanged(int line, int column);
    void onReplaceRequested();
    void onReplaceAllRequested();

    // Recompute the active document's word count (cheap; NoteDocument caches
    // it) and re-ship it on cursorInfoChanged at the current caret so the
    // status bar tracks edits without waiting for a cursor move.
    void refreshWordCount();

    // Link resolution
    QString resolveTarget(const QString &target) const;

    // Receives Markoff::LinkService::linkActivated from either the Live
    // binding's service or the Reading leaf's service, resolves the raw
    // target to a relative path, and re-emits as NoteEditorWidget::linkActivated.
    void onLinkActivated(const Markoff::LinkActivation &activation);

    // --- Mode-transition helpers ---
    int stackIndexFor(ViewMode mode) const;
    void ensureWidgetConstructed(ViewMode mode);

    // Leaf for a given mode (nullptr until lazily constructed). activeLeaf()
    // == leafFor(m_viewMode).
    Markoff::MarkdownView *leafFor(ViewMode mode) const;

    // (activeLeaf is now a public accessor — declared above with editor()/
    // sourceEditor(). Internal callers below also use it.)

    // C2 — apply current theme to every constructed leaf (Live/Source/Reading).
    void applyThemeToAllLeaves();

    // Captures ephemeral state (scroll/cursor/fold) from the current active
    // leaf as a QJsonObject, and packs it into the Corbomite EphemeralState
    // envelope (mode + sourceFlag). Called before detaching the leaf.
    EphemeralState captureEphemeralStateFor(ViewMode mode) const;
    // Restores ephemeral state to the target leaf widget.
    void restoreEphemeralStateFor(ViewMode mode, const EphemeralState &s);

    // Phase 1 (contract v2) — one-time wiring applied to each leaf at
    // construction: theme application now; contextChanged + cursor
    // forwarding are added by the toolbar-state and statusbar tasks.
    // Takes the base pointer: wiring must stay leaf-agnostic.
    void wireLeaf(Markoff::MarkdownView *leaf);

    QStackedWidget *m_stack = nullptr;

    // Shared link service forwarded to all editor leaves (Live + Reading).
    // Owned by this widget; constructed eagerly in NoteEditorWidget().
    Markoff::DefaultLinkService *m_linkService = nullptr;

    // Cluster K — exactly one of m_editor / m_canvasEditor backs the
    // LivePreview slot, decided once at construction from
    // CorbomiteSettings::canvasLivePreview(). Whichever is unused stays
    // nullptr for this widget's lifetime (no runtime engine switching yet —
    // see plan's "Runtime engine switching" open question).
    Markoff::Live::EditorWidget *m_editor = nullptr;
    Markoff::Canvas::EditorWidget *m_canvasEditor = nullptr;
    // Source mode widget — lazy. Constructed on first `setViewMode(Source)`
    // and cached in the stack thereafter. Accessor returns nullptr until
    // first construction. See `ensureWidgetConstructed`.
    Markoff::Source::Editor *m_sourceEditor = nullptr;
    // Reading mode widget — a read-only Markoff::Styled::Editor (QWidget, no
    // QML). Lazy, same pattern as m_sourceEditor. Replaces the retired
    // Markoff::Reading::ReadingView stub.
    Markoff::Styled::Editor *m_styledReadingView = nullptr;
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
    // Tracks the active document's textChanged → refreshWordCount() wiring so
    // it is torn down when the document is swapped or detached.
    QMetaObject::Connection m_wordCountConn;

    // Hover preview (lifetime owned by MainWindow).
    HoverPopover *m_hoverPopover = nullptr;

    // C2 — theme service (lifetime owned by MainWindow).
    Core::ThemeService *m_themeService = nullptr;

    // C4 Task 14 — mermaid renderer (lifetime owned by MainWindow).
    Markoff::MermaidRenderer *m_mermaidRenderer = nullptr;

    FindBar *m_findBar = nullptr;

    // Completion (Phase 2 revival). Leaf-agnostic driver; owns the popup and
    // trigger session. Re-pointed at the active leaf on every mode switch.
    CompletionController *m_completion = nullptr;
};

} // namespace Corbomite
