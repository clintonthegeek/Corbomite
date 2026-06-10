// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
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
#include <QKeyEvent>
#include <QStackedWidget>
#include <QStringListModel>
#include <QVBoxLayout>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_linkService(new Markoff::DefaultLinkService(this))
    , m_stack(new QStackedWidget(this))
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

    // Wire the shared link service into the Live binding so link clicks in
    // Live mode route through it. The service is also set on the Reading leaf
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
    m_editor->installEventFilter(this);
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
}

void NoteEditorWidget::setHoverPopover(HoverPopover *popover)
{
    m_hoverPopover = popover;
}

void NoteEditorWidget::setEditorSuggestManager(EditorSuggestManager *manager)
{
    m_suggestManager = manager;
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
    // TODO(port-foundation-exploration): theme propagation deferred — full
    // theme port disabled (SystemThemeBuilder + ThemeService stubbed out).
    // Live leaf's theme path is binding()->setTheme(...) but Markoff::Theme
    // ctor / setters changed; revisit when the theme feature ports.
}

void NoteEditorWidget::setMermaidRenderer(Markoff::MermaidRenderer *renderer)
{
    m_mermaidRenderer = renderer;
    // TODO(port-foundation-exploration): Markoff::MermaidRenderer abstract
    // retired (E5 work). No-op until restoration.
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
            m_sourceEditor = new Markoff::Source::Editor(this);
            m_sourceIndex = m_stack->addWidget(m_sourceEditor);
            // TODO(port-foundation-exploration): setViewTheme retired (theme port).
        }
        break;
    case ViewMode::LivePreview:
        // Always constructed eagerly in the ctor.
        break;
    case ViewMode::Reading:
        if (!m_styledReadingView) {
            m_styledReadingView = new Markoff::Styled::Editor(this);
            m_styledReadingView->setReadOnly(true);
            // Share the same link service so link activations from the Reading
            // leaf are routed through the same onLinkActivated slot as Live mode.
            m_styledReadingView->setLinkService(m_linkService);
            m_readingIndex = m_stack->addWidget(m_styledReadingView);
        }
        break;
    }
}

Markoff::MarkdownView *NoteEditorWidget::activeLeaf() const
{
    switch (m_viewMode) {
    case ViewMode::Source:
        return m_sourceEditor;   // may be nullptr if not yet constructed
    case ViewMode::LivePreview:
        return m_editor;
    case ViewMode::Reading:
        return m_styledReadingView;  // may be nullptr if not yet constructed
    }
    return nullptr;
}

bool NoteEditorWidget::goToLine(int line)
{
    if (line < 1) return false;
    switch (m_viewMode) {
    case ViewMode::Source:
        if (m_sourceEditor) {
            m_sourceEditor->setCursorPosition({line, 0});
            return true;
        }
        return false;
    case ViewMode::LivePreview:
        // TODO(port-foundation-exploration): Markoff::Live::EditorWidget
        // doesn't expose goToLine. Cursor placement by line/column needs
        // the legacy line-coord → byte-offset / BlockAnchor conversion to
        // be reimplemented against the new MarkoffDocument. No-op for now.
        return false;
    case ViewMode::Reading:
        return false;
    }
    return false;
}

EphemeralState NoteEditorWidget::captureEphemeralStateFor(ViewMode mode) const
{
    EphemeralState s;
    const auto compound = ViewModeSerializer::toCompound(mode);
    s.modeRaw = compound.mode;
    s.sourceFlag = compound.source;

    // TODO(port-foundation-exploration): ephemeral-state capture stubbed —
    // Source::Editor renamed/methods changed (scrollPosition gone), Live
    // EditorWidget doesn't expose cursorLine/cursorColumn, Reading retired.
    // Each branch will be reimplemented when the relevant feature port lands.
    (void)mode;
    return s;
}

void NoteEditorWidget::restoreEphemeralStateFor(ViewMode mode,
                                                 const EphemeralState &s)
{
    // TODO(port-foundation-exploration): ephemeral-state restore stubbed —
    // pairs with the stubbed captureEphemeralStateFor above.
    (void)mode;
    (void)s;
}

void NoteEditorWidget::setViewMode(ViewMode newMode)
{
    if (m_viewMode == newMode) return;

    // 1. Capture outgoing leaf's ephemeral state (scroll/cursor/fold).
    const EphemeralState outgoing = captureEphemeralStateFor(m_viewMode);

    // 2. Detach outgoing leaf from the canonical document.
    if (isFindBarVisible() && m_doc) {
        if (auto *leaf = activeLeaf()) {
            if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
                live->detachFindController();
            else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
                src->detachFindController();
        }
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
    if (isFindBarVisible() && m_doc) {
        auto *fc = m_doc->findController();
        if (auto *leaf = activeLeaf()) {
            if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
                live->attachFindController(fc);
            else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
                src->attachFindController(fc);
        }
    }

    // 5. Restore scroll + fold + cursor (mode-appropriate).
    restoreEphemeralStateFor(newMode, outgoing);

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
    // TODO(port-foundation-exploration): cursorLine retired; line/column
    // accessor needs TextAnchor → line conversion on the new MarkoffDocument.
    return 0;
}

int NoteEditorWidget::currentColumn() const
{
    // TODO(port-foundation-exploration): see currentLine.
    return 0;
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

bool NoteEditorWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_editor) return QWidget::eventFilter(obj, event);

    // Editor focus-out while a popup is up → dismiss.
    if (event->type() == QEvent::FocusOut && m_completionPopup) {
        dismissCompletion();
        return false;
    }

    if (event->type() == QEvent::KeyPress && m_completionPopup) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Down:
            m_completionPopup->selectNext();
            return true;
        case Qt::Key_Up:
            m_completionPopup->selectPrevious();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_completionPopup->acceptCurrent()) return true;
            break;
        case Qt::Key_Escape:
            dismissCompletion();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void NoteEditorWidget::onTextChanged()
{
    // Phase C3: canonical content is authoritative — m_editor pushes changes
    // through MarkdownDelta commands onto the shared QUndoStack, so we no
    // longer need to flush toPlainText() back to m_doc here. We keep this
    // slot only to service completion filter updates.
    if (m_completionPopup) updateCompletionFilter();
}

void NoteEditorWidget::onCursorPositionChanged(int line, int column)
{
    if (!m_doc) return;
    Q_EMIT cursorInfoChanged(line, column, m_cachedWordCount);
}

// --- Completion ---

void NoteEditorWidget::maybeActivateSuggester()
{
    // TODO(port-foundation-exploration): completion dispatch depended on the
    // old Markoff::Editor's cursorLine/cursorColumn/viewport/toPlainText.
    // Completion port is its own feature (separate from the find port).
    // No-op until that feature lands.
}

void NoteEditorWidget::positionCompletionPopup()
{
    // TODO(port-foundation-exploration): see maybeActivateSuggester.
}

int NoteEditorWidget::absoluteCursorPos() const
{
    // TODO(port-foundation-exploration): see maybeActivateSuggester.
    return -1;
}

QString NoteEditorWidget::currentLineText() const
{
    // TODO(port-foundation-exploration): toPlainText retired on EditorWidget.
    return {};
}

void NoteEditorWidget::updateCompletionFilter()
{
    if (!m_completionPopup) return;
    const int absPos = absoluteCursorPos();
    if (absPos < 0 || absPos < m_completionTriggerPos) {
        dismissCompletion();
        return;
    }
    // TODO(port-foundation-exploration): toPlainText accessor retired on
    // EditorWidget — completion port will reimplement against MarkoffDocument.
    dismissCompletion();
    (void)absPos;
}

void NoteEditorWidget::dismissCompletion()
{
    if (m_completionPopup) {
        auto *p = m_completionPopup;
        m_completionPopup = nullptr;
        p->hide();
        p->deleteLater();
    }
    m_activeSuggester = nullptr;
    m_completionTriggerPos = -1;
}

void NoteEditorWidget::onCompletionAccepted(const QString &text, const QString &data)
{
    Q_UNUSED(text)
    Q_UNUSED(data)
    // TODO(port-foundation-exploration): completion-write path used the
    // retired Markoff::MarkdownDelta + undoStack APIs. Port to applyFlatEdit
    // + d2UndoLog when the completion feature lands. No-op for now.
    dismissCompletion();
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
    if (auto *leaf = activeLeaf()) {
        // Use the polymorphic attach hook present on both Live::EditorWidget
        // and Source::Editor. Symmetric API; no leaf-type switch needed in
        // the contract, but the call site needs a downcast since
        // MarkdownView itself doesn't expose attachFindController.
        if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
            live->attachFindController(fc);
        else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
            src->attachFindController(fc);
    }
    fc->activate();
    m_findBar->show();
    m_findBar->focusLineEdit();
}

void NoteEditorWidget::hideFindBar()
{
    if (m_doc) {
        if (auto *leaf = activeLeaf()) {
            if (auto *live = qobject_cast<Markoff::Live::EditorWidget*>(leaf))
                live->detachFindController();
            else if (auto *src = qobject_cast<Markoff::Source::Editor*>(leaf))
                src->detachFindController();
        }
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
