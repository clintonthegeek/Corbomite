// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionController.h"
#include "FindBar.h"
#include "LineResolve.h"
#include "HoverPopover.h"
#include "ViewModeSerializer.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ThemeService.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/EphemeralState.h"
#include "corbomitesettings.h"
#include "dialogs/QuickSwitcherModel.h"

#include <markoff/canvas/EditorWidget.h>
#include <markoff/canvas/View.h>
#include <markoff/core/DefaultLinkService.h>
#include <markoff/core/FindController.h>
#include <markoff/core/SearchEngine.h>
#include <markoff/core/LinkActivation.h>
#include <markoff/core/LinkKind.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/Origin.h>
#include <markoff/live/EditorWidget.h>
#include <markoff/live/LiveListModelBinding.h>
#include <markoff/source/Editor.h>
#include <markoff/styled/Editor.h>
// TODO(port-foundation-exploration): old Markoff::Editor / MarkdownDelta /
// Markoff::Reading / Markoff::MermaidRenderer all retired with the leaf
// reshuffling — the live leaf is now hosted via Markoff::Live::EditorWidget
// (this commit). Many of the editor's old signals (textChanged,
// wordCountChanged, linkClicked, linkHovered, completionDismissHint,
// cursorPositionChanged-with-line/col) have no direct equivalents on
// LiveListModelBinding yet; their wiring is stubbed and will be reinstated
// as feature ports land.

#include <QCursor>
#include <QDesktopServices>
#include <QFileInfo>
#include <QStackedWidget>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_linkService(new Markoff::DefaultLinkService(this))
    , m_stack(new QStackedWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    m_findBar = new FindBar(this);
    m_findBar->hide();
    layout->addWidget(m_findBar);
    QObject::connect(m_findBar, &FindBar::closeRequested,
                     this, &NoteEditorWidget::hideFindBar);
    connect(m_findBar, &FindBar::replaceRequested,
            this, &NoteEditorWidget::onReplaceRequested);
    connect(m_findBar, &FindBar::replaceAllRequested,
            this, &NoteEditorWidget::onReplaceAllRequested);

    // Cluster K — LivePreview backend decided once at construction from the
    // settings toggle (restart-to-apply; no runtime engine switching yet).
    // Exactly one of m_editor / m_canvasEditor is constructed; the other
    // stays nullptr for this widget's lifetime.
    Markoff::MarkdownView *livePreviewLeaf = nullptr;
    if (CorbomiteSettings::self()->canvasLivePreview()) {
        // leaf-specific: canvas widget construction (Cluster K, experimental).
        m_canvasEditor = new Markoff::Canvas::EditorWidget(this);
        livePreviewLeaf = m_canvasEditor;
        m_livePreviewIndex = m_stack->addWidget(m_canvasEditor);
        // Same "reference, not owner" link-service wiring as Live's
        // binding()->setLinkService and Reading's setLinkService — just
        // reached through the composed View escape hatch, per
        // Markoff::Canvas::EditorWidget::view()'s doc comment.
        m_canvasEditor->view()->setLinkService(m_linkService);
        // Inline title band (rename-via-header): only canvas exposes this,
        // so wiring lives here rather than in the leaf-agnostic wireLeaf().
        connect(m_canvasEditor, &Markoff::Canvas::EditorWidget::titleEdited,
                this, &NoteEditorWidget::onTitleEdited);
        applyReadableLineWidth(CorbomiteSettings::self()->readableLineWidth());
        m_titleRenameTimer = new QTimer(this);
        m_titleRenameTimer->setSingleShot(true);
        m_titleRenameTimer->setInterval(600);
        connect(m_titleRenameTimer, &QTimer::timeout, this, [this]() {
            Q_EMIT titleRenameRequested(m_pendingTitleRename);
        });
    } else {
        // leaf-specific: Live QML leaf construction — revisit if the canonical
        // live view changes leaf class (user directive 2026-06-10).
        m_editor = new Markoff::Live::EditorWidget(
            Markoff::Live::LiveListModelBinding::AllCapabilities, this);
        livePreviewLeaf = m_editor;
        m_livePreviewIndex = m_stack->addWidget(m_editor);
        // leaf-specific: Live QML binding wiring — the shared link service
        // routes link clicks in Live mode. The service is also set on the
        // Reading leaf in ensureWidgetConstructed(Reading).
        m_editor->binding()->setLinkService(m_linkService);
    }
    m_stack->setCurrentIndex(m_livePreviewIndex);

    connect(m_linkService, &Markoff::LinkService::linkActivated,
            this, &NoteEditorWidget::onLinkActivated);

    // Hover preview (2026-06-11) — forward the shared LinkService hover
    // stream to the host-owned popover. Both Live and Reading leaves emit
    // through this one service, so this covers both. m_hoverPopover is set
    // later by the host (setHoverPopover), so read it lazily at signal time.
    connect(m_linkService, &Markoff::LinkService::linkHovered, this,
            [this](const Markoff::LinkActivation &act, const QPoint &globalPos) {
                if (!m_hoverPopover) return;
                if (act.kind == Markoff::LinkKind::External) return;
                const QString target =
                    !act.page.isEmpty() ? act.page : act.rawText;
                if (target.isEmpty()) return;
                m_hoverPopover->scheduleShow(target, globalPos);
            });
    connect(m_linkService, &Markoff::LinkService::linkHoverLeft, this,
            [this](const QString & /*linkText*/) {
                // The popover tracks a single active target, so cancellation is
                // global — the specific link we left doesn't matter here.
                if (m_hoverPopover) m_hoverPopover->linkHoverEnded();
            });

    // TODO(port-foundation-exploration): old Markoff::Editor exposed
    // textChanged / cursorPositionChanged(int line, int col) /
    // wordCountChanged / linkClicked / linkHovered / completionDismissHint.
    // None of these has a direct equivalent on Live's LiveListModelBinding
    // yet. Cursor info will need to come from binding()->cursorState()
    // via cursorChanged(); text/word/link signals need ad-hoc wiring through
    // MarkoffDocument or LinkService. Each will be hooked back up as its
    // feature ports — find UI first.

    wireLeaf(livePreviewLeaf);

    // Phase 2 completion revival — leaf-agnostic driver. Re-pointed at the
    // active leaf on every mode switch (see setViewMode). The Live leaf is
    // valid here, so seed it immediately.
    m_completion = new CompletionController(this);
    m_completion->setLeaf(activeLeaf());
}

void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    if (m_doc == doc) return;

    // Detach active leaf from the old document.
    if (auto *leaf = activeLeaf()) {
        leaf->setDocument(nullptr);
    }

    // Drop the previous document's word-count wiring before swapping.
    disconnect(m_wordCountConn);
    disconnect(m_pathChangedConn);

    m_doc = doc;

    if (m_doc) {
        // TODO(port-foundation-exploration): setResourceProvider lived on the
        // old Markoff::Editor; on Live's binding it's not directly exposed.
        // Resource resolution now flows through services (Markoff::Vault::
        // ResourceProvider, MarkoffServices). Wire-up deferred to a follow-up
        // micro-spec.
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            // TODO(port): plug m_resourceProvider into the document's services.
        }

        // Attach the active leaf to the new document.
        if (auto *leaf = activeLeaf()) {
            leaf->setDocument(m_doc->markoff());
        }

        // Status-bar word count: track every edit (NoteDocument caches the
        // count and invalidates on change) and seed the initial value so the
        // count is correct the moment the document opens, without waiting for
        // a cursor move.
        m_wordCountConn = connect(m_doc, &NoteDocument::textChanged,
                                  this, &NoteEditorWidget::refreshWordCount);
        refreshWordCount();

        // Re-seed the title band on an external rename (file explorer,
        // another leaf's rename dialog) while this note is open.
        m_pathChangedConn = connect(m_doc, &NoteDocument::pathChanged,
                                    this, &NoteEditorWidget::syncInlineTitleForCanvas);
    } else {
        m_cachedWordCount = 0;
        // TODO(port-foundation-exploration): no equivalent of Editor::clear()
        // on EditorWidget; closing the document detaches via setDocument(nullptr).
        // Redundant with the activeLeaf() detach above when Live/canvas is the
        // active leaf; guarded because m_editor is nullptr in canvas-engine mode
        // (Cluster K — exactly one of m_editor/m_canvasEditor is constructed).
        if (m_editor) m_editor->setDocument(nullptr);
    }

    syncInlineTitleForCanvas();

    if (m_completion) m_completion->setNoteDocument(m_doc);
}

void NoteEditorWidget::setHoverPopover(HoverPopover *popover)
{
    m_hoverPopover = popover;
}

void NoteEditorWidget::setEditorSuggestManager(EditorSuggestManager *manager)
{
    if (m_completion) m_completion->setManager(manager);
}

void NoteEditorWidget::setThemeService(Core::ThemeService *service)
{
    if (m_themeService == service) return;
    if (m_themeService)
        disconnect(m_themeService, nullptr, this, nullptr);
    m_themeService = service;
    if (!m_themeService) return;
    connect(m_themeService, &Core::ThemeService::themeChanged,
            this, [this](const Markoff::Theme &) { applyThemeToAllLeaves(); });
    applyThemeToAllLeaves();
}

void NoteEditorWidget::applyThemeToAllLeaves()
{
    if (!m_themeService) return;
    const Markoff::Theme t = m_themeService->currentTheme();
    // Cluster K: m_canvasEditor is included alongside m_editor — exactly one
    // of the pair is non-null (see ctor), so this stays a flat literal list
    // rather than a loop, matching the existing convention.
    const std::initializer_list<Markoff::MarkdownView *> leaves{
        m_editor, m_canvasEditor, m_sourceEditor, m_styledReadingView};
    for (Markoff::MarkdownView *view : leaves)
        if (view) view->setTheme(t);
}

void NoteEditorWidget::wireLeaf(Markoff::MarkdownView *leaf)
{
    if (m_themeService)
        leaf->setTheme(m_themeService->currentTheme());
    connect(leaf, &Markoff::MarkdownView::contextChanged, this,
            [this, leaf](const Markoff::EditorContext &ctx) {
                if (leaf == activeLeaf())
                    Q_EMIT editorContextChanged(ctx);
            });
    connect(leaf, &Markoff::MarkdownView::cursorPositionChanged, this,
            [this, leaf](int line, int column) {
                if (leaf == activeLeaf())
                    onCursorPositionChanged(line, column);
            });
}

void NoteEditorWidget::setMermaidRenderer(Markoff::MermaidRenderer *renderer)
{
    m_mermaidRenderer = renderer;
    // leaf-specific (when restored): mermaid-renderer injection is per-leaf
    // wiring. TODO(port-foundation-exploration): Markoff::MermaidRenderer
    // abstract retired (E5 work). No-op until restoration.
    (void)renderer;
}

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

void NoteEditorWidget::setVault(Vault *vault)
{
    m_vault = vault;
    if (m_doc && m_vault) {
        // TODO(port-foundation-exploration): setResourceProvider on Live
        // editor not yet exposed — see setNoteDocument above. Resource
        // provider is constructed for future wire-up.
        delete m_resourceProvider;
        m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
    }
}

// =========================================================================
// Mode transition
// =========================================================================

int NoteEditorWidget::stackIndexFor(ViewMode mode) const
{
    switch (mode) {
    case ViewMode::Source:      return m_sourceIndex;
    case ViewMode::LivePreview: return m_livePreviewIndex;
    case ViewMode::Reading:     return m_readingIndex;
    }
    return m_livePreviewIndex;
}

void NoteEditorWidget::ensureWidgetConstructed(ViewMode mode)
{
    switch (mode) {
    case ViewMode::Source:
        if (!m_sourceEditor) {
            // leaf-specific: Source leaf construction.
            m_sourceEditor = new Markoff::Source::Editor(this);
            m_sourceIndex = m_stack->addWidget(m_sourceEditor);
            wireLeaf(m_sourceEditor);
        }
        break;
    case ViewMode::LivePreview:
        // Always constructed eagerly in the ctor.
        break;
    case ViewMode::Reading:
        if (!m_styledReadingView) {
            // leaf-specific: Reading is a read-only Styled leaf; it shares
            // the link service so link activations route through the same
            // onLinkActivated slot as Live mode.
            m_styledReadingView = new Markoff::Styled::Editor(this);
            m_styledReadingView->setReadOnly(true);
            m_styledReadingView->setLinkService(m_linkService);
            m_readingIndex = m_stack->addWidget(m_styledReadingView);
            wireLeaf(m_styledReadingView);
        }
        break;
    }
}

Markoff::MarkdownView *NoteEditorWidget::leafFor(ViewMode mode) const
{
    switch (mode) {
    case ViewMode::Source:      return m_sourceEditor;
    // Cluster K: exactly one of m_editor/m_canvasEditor is non-null.
    case ViewMode::LivePreview: return m_editor ? static_cast<Markoff::MarkdownView *>(m_editor)
                                                 : static_cast<Markoff::MarkdownView *>(m_canvasEditor);
    case ViewMode::Reading:     return m_styledReadingView;
    }
    return nullptr;
}

Markoff::MarkdownView *NoteEditorWidget::activeLeaf() const
{
    return leafFor(m_viewMode);
}

bool NoteEditorWidget::goToLine(int line)
{
    if (line < 1) return false;
    Markoff::MarkdownView *leaf = activeLeaf();
    if (!leaf) return false;
    leaf->setCursorPosition({line, 1});
    return true;
}

EphemeralState NoteEditorWidget::captureEphemeralStateFor(ViewMode mode) const
{
    EphemeralState s;
    const auto compound = ViewModeSerializer::toCompound(mode);
    s.modeRaw = compound.mode;
    s.sourceFlag = compound.source;

    // Contract v2: CursorPos is 1-based flat visual lines (0 = unset);
    // scroll is the 0.0–1.0 fraction from scrollPositionVisualLine().
    if (Markoff::MarkdownView *leaf = leafFor(mode)) {
        const Markoff::CursorPos pos = leaf->cursorPosition();
        s.cursor.line = pos.line;
        s.cursor.column = pos.column;
        s.scroll = leaf->scrollPositionVisualLine();
    }
    return s;
}

void NoteEditorWidget::restoreEphemeralStateFor(ViewMode mode,
                                                 const EphemeralState &s)
{
    Markoff::MarkdownView *leaf = leafFor(mode);
    if (!leaf) return;
    if (s.cursor.line >= 1)
        leaf->setCursorPosition({s.cursor.line, std::max(1, s.cursor.column)});
    leaf->setScrollPositionVisualLine(std::clamp(s.scroll, 0.0f, 1.0f));
}

void NoteEditorWidget::setViewMode(ViewMode newMode)
{
    if (m_viewMode == newMode) return;

    // 1. Capture outgoing leaf's ephemeral state (scroll/cursor/fold).
    const EphemeralState outgoing = captureEphemeralStateFor(m_viewMode);

    // 2. Detach outgoing leaf from the canonical document.
    if (isFindBarVisible() && m_doc) {
        if (auto *leaf = activeLeaf())
            leaf->detachFindController();
    }
    if (auto *leaf = activeLeaf()) {
        leaf->setDocument(nullptr);
    }

    // 3. Swap the active mode + stack page. Lazy-construct if needed.
    m_viewMode = newMode;
    ensureWidgetConstructed(newMode);
    const int idx = stackIndexFor(newMode);
    if (idx >= 0) m_stack->setCurrentIndex(idx);

    // 4. Attach the incoming leaf to the canonical document.
    if (auto *leaf = activeLeaf()) {
        if (m_doc) {
            leaf->setDocument(m_doc->markoff());
        }
    }
    syncInlineTitleForCanvas();

    // Re-point the completion driver at the now-active leaf (dismisses any
    // open session). Leaf-agnostic — base pointer only.
    if (m_completion) m_completion->setLeaf(activeLeaf());

    if (isFindBarVisible() && m_doc) {
        auto *fc = m_doc->findController();
        if (auto *leaf = activeLeaf())
            leaf->attachFindController(fc);   // after setDocument, per contract
    }

    // 5. Restore scroll + fold + cursor (mode-appropriate).
    restoreEphemeralStateFor(newMode, outgoing);

    // Refresh statusbar cursor info for the incoming leaf.
    if (auto *leaf = activeLeaf()) {
        const Markoff::CursorPos pos = leaf->cursorPosition();
        onCursorPositionChanged(pos.line, pos.column);
    }

    Q_EMIT viewModeChanged(newMode);
}

NoteEditorWidget::ViewMode NoteEditorWidget::viewMode() const
{
    return m_viewMode;
}

Markoff::Live::EditorWidget *NoteEditorWidget::editor() const
{
    return m_editor;
}

Markoff::Source::Editor *NoteEditorWidget::sourceEditor() const
{
    return m_sourceEditor;
}

int NoteEditorWidget::currentLine() const
{
    auto *leaf = activeLeaf();
    return leaf ? leaf->cursorPosition().line : 0;
}

int NoteEditorWidget::currentColumn() const
{
    auto *leaf = activeLeaf();
    return leaf ? leaf->cursorPosition().column : 0;
}

// Cluster E Phase 1 — ephemeral state round-trip. Now called from
// EditorViewManager's buildPaneLayout / applyPaneLayout so each per-leaf
// eState round-trips to workspace.json.
EphemeralState NoteEditorWidget::saveEphemeralState() const
{
    return captureEphemeralStateFor(m_viewMode);
}

void NoteEditorWidget::restoreEphemeralState(const EphemeralState &state)
{
    const auto mode = ViewModeSerializer::fromCompound(state.modeRaw,
                                                       std::optional<bool>{state.sourceFlag});
    // Drive the full transition through setViewMode so the ensure/attach/restore
    // sequencing stays consistent. If the mode is the same, still push state
    // through `restoreEphemeralStateFor` directly.
    if (mode != m_viewMode) {
        setViewMode(mode);
    }
    ensureWidgetConstructed(mode);
    restoreEphemeralStateFor(mode, state);
}

void NoteEditorWidget::onCursorPositionChanged(int line, int column)
{
    if (!m_doc) return;
    Q_EMIT cursorInfoChanged(line, column, m_cachedWordCount);
}

void NoteEditorWidget::refreshWordCount()
{
    if (!m_doc) {
        m_cachedWordCount = 0;
        return;
    }
    m_cachedWordCount = m_doc->wordCount();
    const Markoff::CursorPos pos =
        activeLeaf() ? activeLeaf()->cursorPosition() : Markoff::CursorPos{};
    Q_EMIT cursorInfoChanged(pos.line, pos.column, m_cachedWordCount);
}

void NoteEditorWidget::syncInlineTitleForCanvas()
{
    // Only the canvas leaf has an inline title band today — a no-op on the
    // QML engine keeps this call safe to sprinkle at every doc attach/
    // detach/rename point without a leaf-type check at each call site.
    if (!m_canvasEditor) return;

    if (!m_doc) {
        m_canvasEditor->setInlineTitleVisible(false);
        return;
    }
    m_canvasEditor->setInlineTitle(QFileInfo(m_doc->relativePath()).completeBaseName());
    m_canvasEditor->setInlineTitleVisible(true);
}

void NoteEditorWidget::applyReadableLineWidth(bool readable)
{
    if (!m_canvasEditor) return;
    m_canvasEditor->setContentWidthPolicy(
        readable ? Markoff::Canvas::ContentWidthPolicy::fixedColumn(700.0)
                 : Markoff::Canvas::ContentWidthPolicy::fullWidth());
}

void NoteEditorWidget::onTitleEdited(const QString &newTitle)
{
    // Debounced — see m_titleRenameTimer's doc comment (NoteEditorWidget.h).
    // The band's own text IS the new title (not a path); the eventual
    // titleRenameRequested is a pure forward — MainWindow resolves it
    // against the current note's folder/extension and performs the actual
    // rename via FileManager, the same way the file-explorer's
    // context-menu rename does, just without the confirmation dialog
    // (matching Obsidian's live-typed-header UX).
    m_pendingTitleRename = newTitle;
    if (m_titleRenameTimer) m_titleRenameTimer->start();
}

bool NoteEditorWidget::insertAtCursor(const QString &text, const QString &caretMarker)
{
    if (!m_doc || !m_doc->markoff()) return false;
    Markoff::MarkoffDocument *mdoc = m_doc->markoff();
    Markoff::MarkdownView *leaf = activeLeaf();

    const Markoff::CursorPos pos =
        leaf ? leaf->cursorPosition() : Markoff::CursorPos{};

    // Insert point in applyFlatEdit's no-separator coordinate space. If the
    // caret line can't be resolved, fall back to end-of-document (the old
    // append behaviour) rather than dropping the edit.
    uint32_t at;
    if (const auto off = LineResolve::globalByteOffsetForCursor(mdoc, pos.line, pos.column)) {
        at = *off;
    } else {
        uint32_t total = 0;
        for (const auto &id : mdoc->iterateBlocks())
            total += uint32_t(mdoc->blockText(id).size());
        at = total;
    }

    // Strip the cursor marker (if any); the caret lands where it was, else at
    // the end of the inserted text.
    QString body = text;
    int caretCharIdx = caretMarker.isEmpty() ? -1 : int(body.indexOf(caretMarker));
    if (caretCharIdx >= 0)
        body.remove(caretCharIdx, caretMarker.size());
    else
        caretCharIdx = int(body.length());

    // One undo-integrated transaction; propagates to every leaf. NB:
    // applyFlatEdit canonicalises the inserted text (collapses newline-runs,
    // never creates empty blocks), so a structural break at a block boundary
    // is absorbed rather than producing a blank paragraph — text-insertion
    // semantics, matching the flat-view leaves' own edit path.
    mdoc->applyFlatEdit(at, at, body.toUtf8(), Markoff::Origin::UserEdit);

    // Deterministic caret — never trust a leaf's own post-edit cursor (mirrors
    // CompletionController). The flat-line arithmetic is the pure, unit-tested
    // LineResolve::caretAfterFlatInsert.
    if (Markoff::MarkdownView *l = activeLeaf())
        l->setCursorPosition(
            LineResolve::caretAfterFlatInsert(pos, body.left(caretCharIdx)));
    return true;
}

// --- Link Resolution ---

QString NoteEditorWidget::resolveTarget(const QString &target) const
{
    if (target.isEmpty()) return {};
    if (target.endsWith(QStringLiteral(".md")) || target.endsWith(QStringLiteral(".canvas")))
        return target;
    return target + QStringLiteral(".md");
}

void NoteEditorWidget::onLinkActivated(const Markoff::LinkActivation &activation)
{
    // External URLs (http/https/mailto) are opened by the OS browser.
    if (activation.kind == Markoff::LinkKind::External) {
        QDesktopServices::openUrl(activation.resolvedTarget);
        return;
    }

    // For wikilinks, `page` is the resolved note name (e.g. "NoteName" or
    // "folder/NoteName"). For plain markdown links to .md/.canvas files, use
    // `rawText`. Build a raw target and let MainWindow's onNoteActivated or its
    // link-resolver-aware override handle vault-wide disambiguation.
    QString rawTarget;
    if (!activation.page.isEmpty()) {
        // WikiLink or Embed — page is the note name, section is the heading.
        rawTarget = activation.page;
    } else if (!activation.rawText.isEmpty()) {
        rawTarget = activation.rawText;
    } else {
        return;
    }

    // Emit the resolved path signal. The receiver (MainWindow) is responsible
    // for vault-aware disambiguation via LinkResolver. openInNewTab carries
    // through unchanged — this method's whole job is target resolution, not
    // tab-placement policy.
    Q_EMIT linkActivated(rawTarget, activation.openInNewTab);
}

// --- Find UI ---

void NoteEditorWidget::showFindBar()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    m_findBar->setController(fc);
    // Polymorphic attach on the MarkdownView base (contract v2) — works for
    // all three leaves; Reading (styled) gains find with this call. Must be
    // called after setDocument (brief §2 find-attach behavioral note).
    if (auto *leaf = activeLeaf())
        leaf->attachFindController(fc);
    fc->activate();
    m_findBar->show();
    m_findBar->focusLineEdit();
    m_findBar->setReplaceMode(false);
}

void NoteEditorWidget::hideFindBar()
{
    if (m_doc) {
        if (auto *leaf = activeLeaf())
            leaf->detachFindController();
        m_doc->findController()->deactivate();
    }
    m_findBar->hide();
    if (auto *leaf = activeLeaf()) leaf->setFocus();
}

bool NoteEditorWidget::isFindBarVisible() const
{
    return m_findBar && m_findBar->isVisible();
}

void NoteEditorWidget::showReplaceBar()
{
    showFindBar();                  // shares attach/activate/focus path
    m_findBar->setReplaceMode(true);
}

void NoteEditorWidget::onReplaceRequested()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    const int idx = fc->currentMatchIndex();
    if (idx < 0 || idx >= fc->matchCount()) return;
    const auto cur = fc->matches().at(idx);
    const QString repl = m_findBar->replacementText();

    m_doc->markoff()->replaceMatches(
        { Markoff::SearchHit{ cur.block, cur.byteOffset, cur.byteLength } },
        repl);
    // replaceMatches flushes synchronously, so fc has already recomputed.
    // Advance past the inserted replacement (avoids re-selecting a replacement
    // that itself contains the needle).
    fc->selectMatchAtOrAfter(cur.block,
                             cur.byteOffset + static_cast<quint32>(repl.toUtf8().size()));
}

void NoteEditorWidget::onReplaceAllRequested()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    QList<Markoff::SearchHit> hits;
    hits.reserve(fc->matchCount());
    for (const auto &m : fc->matches())
        hits.append(Markoff::SearchHit{ m.block, m.byteOffset, m.byteLength });
    m_doc->markoff()->replaceMatches(hits, m_findBar->replacementText());
}

} // namespace Corbomite
