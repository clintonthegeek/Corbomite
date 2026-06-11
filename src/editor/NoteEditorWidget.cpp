// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionController.h"
#include "FindBar.h"
#include "HoverPopover.h"
#include "ViewModeSerializer.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ThemeService.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/EphemeralState.h"
#include "dialogs/QuickSwitcherModel.h"

#include <markoff/core/DefaultLinkService.h>
#include <markoff/core/FindController.h>
#include <markoff/core/LinkActivation.h>
#include <markoff/core/LinkKind.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/core/MarkoffDocument.h>
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
#include <QStackedWidget>
#include <QStringListModel>
#include <QVBoxLayout>

#include <algorithm>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_linkService(new Markoff::DefaultLinkService(this))
    , m_stack(new QStackedWidget(this))
    // leaf-specific: Live QML leaf construction — revisit if the canonical
    // live view changes leaf class (user directive 2026-06-10).
    , m_editor(new Markoff::Live::EditorWidget(
          Markoff::Live::LiveListModelBinding::AllCapabilities, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    m_findBar = new FindBar(this);
    m_findBar->hide();
    layout->addWidget(m_findBar);
    QObject::connect(m_findBar, &FindBar::closeRequested,
                     this, &NoteEditorWidget::hideFindBar);

    m_livePreviewIndex = m_stack->addWidget(m_editor);
    m_stack->setCurrentIndex(m_livePreviewIndex);

    // leaf-specific: Live QML binding wiring — the shared link service routes
    // link clicks in Live mode. The service is also set on the Reading leaf
    // in ensureWidgetConstructed(Reading).
    m_editor->binding()->setLinkService(m_linkService);
    connect(m_linkService, &Markoff::LinkService::linkActivated,
            this, &NoteEditorWidget::onLinkActivated);

    // TODO(port-foundation-exploration): old Markoff::Editor exposed
    // textChanged / cursorPositionChanged(int line, int col) /
    // wordCountChanged / linkClicked / linkHovered / completionDismissHint.
    // None of these has a direct equivalent on Live's LiveListModelBinding
    // yet. Cursor info will need to come from binding()->cursorState()
    // via cursorChanged(); text/word/link signals need ad-hoc wiring through
    // MarkoffDocument or LinkService. Each will be hooked back up as its
    // feature ports — find UI first.

    wireLeaf(m_editor);

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
    } else {
        // TODO(port-foundation-exploration): no equivalent of Editor::clear()
        // on EditorWidget; closing the document detaches via setDocument(nullptr).
        m_editor->setDocument(nullptr);
    }

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
    const std::initializer_list<Markoff::MarkdownView *> leaves{
        m_editor, m_sourceEditor, m_styledReadingView};
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
    case ViewMode::LivePreview: return m_editor;
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
    // for vault-aware disambiguation via LinkResolver.
    Q_EMIT linkActivated(rawTarget);
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

} // namespace Corbomite
