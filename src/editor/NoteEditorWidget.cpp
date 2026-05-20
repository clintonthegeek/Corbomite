// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
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

// TODO(port): old Markoff::Editor retired
// include <markoff/Editor.h>
#include <markoff/core/MarkdownView.h>
// TODO(port): MarkdownDelta retired
// include <markoff/MarkdownDelta.h>
#include <markoff/core/MarkoffDocument.h>
// TODO(port): Markoff::MermaidRenderer retired (E5 work)
#include <markoff/source/Editor.h>
// TODO(port): Reading retired
// include <markoff/reading/ReadingView.h>

#include <QCursor>
#include <QKeyEvent>
#include <QStackedWidget>
#include <QStringListModel>
#include <QVBoxLayout>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
    , m_editor(new Markoff::Editor(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    // Markoff (LivePreview) is the default mode for most users so construct
    // it eagerly + mount it as the first stack page. Source and Reading
    // widgets are constructed lazily on first `setViewMode(...)` and cached
    // in the stack thereafter (see `ensureWidgetConstructed`).
    m_livePreviewIndex = m_stack->addWidget(m_editor);
    m_stack->setCurrentIndex(m_livePreviewIndex);

    connect(m_editor, &Markoff::Editor::textChanged,
            this, &NoteEditorWidget::onTextChanged);
    connect(m_editor, &Markoff::Editor::cursorPositionChanged,
            this, &NoteEditorWidget::onCursorPositionChanged);
    connect(m_editor, &Markoff::Editor::wordCountChanged,
            this, [this](int count) { m_cachedWordCount = count; });
    connect(m_editor, &Markoff::Editor::linkClicked,
            this, [this](const QString &target) {
        Q_EMIT linkActivated(resolveTarget(target));
    });
    connect(m_editor, &Markoff::Editor::linkHovered,
            this, [this](const QString &target, const QPoint &globalPos) {
        if (!m_hoverPopover) return;
        if (target.isEmpty()) {
            m_hoverPopover->linkHoverEnded();
        } else {
            // Phase C5: Markoff now supplies the global-screen hover
            // position directly. We preserve the +20 y-offset so the
            // popover doesn't land under the cursor and trigger
            // leaveEvent on the hovered link.
            m_hoverPopover->scheduleShow(resolveTarget(target),
                                          globalPos + QPoint(0, 20));
        }
    });
    connect(m_editor, &Markoff::Editor::completionDismissHint,
            this, &NoteEditorWidget::dismissCompletion);
    // Cluster H Phase 3 — trigger detection moved into each EditorSuggest;
    // the editor merely fires cursor changes which the manager dispatches
    // to its registered suggester list (insertion-order first-wins).
    connect(m_editor, &Markoff::Editor::cursorPositionChanged,
            this, [this](int, int) { maybeActivateSuggester(); });

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
        // Update VaultResourceProvider for the live editor.
        m_editor->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            m_editor->setResourceProvider(m_resourceProvider);
        }

        // Attach the active leaf to the new document.
        if (auto *leaf = activeLeaf()) {
            leaf->setDocument(m_doc->markoff());
        }
    } else {
        // No document — clear the live editor (it's always constructed).
        m_editor->clear();
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
    const Markoff::Theme theme = m_themeService->currentTheme();
    if (m_editor)        m_editor->setViewTheme(theme);
    if (m_sourceEditor)  m_sourceEditor->setViewTheme(theme);
    if (m_readingView)   m_readingView->setViewTheme(theme);
}

void NoteEditorWidget::setMermaidRenderer(Markoff::MermaidRenderer *renderer)
{
    m_mermaidRenderer = renderer;
    // Live leaf is always constructed eagerly — inject immediately.
    if (m_editor)
        m_editor->setMermaidRenderer(renderer);
    // Reading leaf is lazy; if already constructed, inject now.
    // If not yet constructed, ensureWidgetConstructed will call
    // setMermaidRenderer when it creates the ReadingView (see below).
    if (m_readingView)
        m_readingView->setMermaidRenderer(renderer);
}

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

void NoteEditorWidget::setVault(Vault *vault)
{
    m_vault = vault;
    if (m_doc && m_vault) {
        m_editor->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
        m_editor->setResourceProvider(m_resourceProvider);
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
            if (m_themeService)
                m_sourceEditor->setViewTheme(m_themeService->currentTheme());
        }
        break;
    case ViewMode::LivePreview:
        // Always constructed eagerly in the ctor.
        break;
    case ViewMode::Reading:
        if (!m_readingView) {
            m_readingView = new Markoff::Reading::ReadingView(this);
            m_readingIndex = m_stack->addWidget(m_readingView);
            if (m_themeService)
                m_readingView->setViewTheme(m_themeService->currentTheme());
            // C4 Task 14: inject mermaid renderer into late-constructed Reading leaf.
            if (m_mermaidRenderer)
                m_readingView->setMermaidRenderer(m_mermaidRenderer);
            // Phase C5: wire Reading-mode link-hover into the same
            // HoverPopover instance the editor uses. Prior to C5
            // Reading mode had no hover popover; wiki-link hover in
            // Reading now shows the preview consistent with Live mode.
            connect(m_readingView, &Markoff::Reading::ReadingView::linkHovered,
                    this, [this](const QString &href, const QPoint &globalPos) {
                if (!m_hoverPopover) return;
                if (href.isEmpty()) {
                    m_hoverPopover->cancel();
                } else {
                    m_hoverPopover->scheduleShow(resolveTarget(href),
                                                  globalPos + QPoint(0, 20));
                }
            });
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
        return m_readingView;    // may be nullptr if not yet constructed
    }
    return nullptr;
}

bool NoteEditorWidget::goToLine(int line)
{
    if (line < 1) return false;
    switch (m_viewMode) {
    case ViewMode::Source:
        if (m_sourceEditor) {
            // Markoff::Source::Editor::setCursorPosition takes 1-based line.
            m_sourceEditor->setCursorPosition({line, 0});
            return true;
        }
        return false;
    case ViewMode::LivePreview:
        if (m_editor) {
            m_editor->goToLine(line);
            return true;
        }
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

    switch (mode) {
    case ViewMode::Source:
        if (m_sourceEditor) {
            s.scroll = m_sourceEditor->scrollPosition();
            // Markoff::Source::Editor uses 1-based lines; EphemeralState
            // stores 0-based (same convention as LivePreview capture below).
            const auto cp = m_sourceEditor->cursorPosition();
            s.cursor.line   = std::max(0, cp.line - 1);
            s.cursor.column = cp.column;
            // foldedHeadings() returns QVector<FoldSpec> — Phase A stub returns
            // empty; EphemeralState holds QVector<int>. Leave as empty for now.
        }
        break;
    case ViewMode::LivePreview:
        s.scroll = m_editor->scrollPositionVisualLine();
        // Markoff exposes 1-based line/column; EphemeralState stores
        // 0-based so Source and LivePreview share a common cursor coord
        // system across transitions. Subtract 1 on save, add on restore.
        s.cursor.line = std::max(0, m_editor->cursorLine() - 1);
        s.cursor.column = std::max(0, m_editor->cursorColumn() - 1);
        break;
    case ViewMode::Reading:
        if (m_readingView) {
            s.scroll = m_readingView->scrollPositionVisualLine();
            s.foldedHeadings = m_readingView->foldedHeadingLines();
            // Stash the line count alongside so restore can detect a
            // structural shift (external edit added/removed lines) and
            // drop folds rather than carry them forward against the wrong
            // headings. Audit: editor-markdown.md §"Other" — `setFoldedHeadingLines`
            // doesn't invalidate when line count changes.
            if (m_doc) {
                s.extraKeys.insert(
                    QStringLiteral("corbomite.foldedHeadingsLineCount"),
                    m_doc->markdown().count(QLatin1Char('\n')) + 1);
            }
        }
        break;
    }

    return s;
}

void NoteEditorWidget::restoreEphemeralStateFor(ViewMode mode,
                                                 const EphemeralState &s)
{
    switch (mode) {
    case ViewMode::Source:
        if (m_sourceEditor) {
            // EphemeralState stores 0-based; Markoff::Source::Editor
            // takes 1-based lines — add 1 on restore.
            m_sourceEditor->setCursorPosition({s.cursor.line + 1, s.cursor.column});
            m_sourceEditor->setScrollPosition(s.scroll);
            // foldedHeadings — Phase A stub; no-op.
        }
        break;
    case ViewMode::LivePreview:
        // Line is 1-based on the Markoff::Editor wire, 0-based in
        // EphemeralState. Column is 0-based on both. goToLineAndColumn
        // preserves the user's in-line cursor position across Source ->
        // Live transitions; before v0.6.1 this fell back to goToLine
        // which always placed the cursor at column 0.
        if (s.cursor.line > 0 || s.cursor.column > 0) {
            m_editor->goToLineAndColumn(s.cursor.line + 1, s.cursor.column);
        }
        m_editor->setScrollPositionVisualLine(s.scroll);
        break;
    case ViewMode::Reading:
        if (m_readingView) {
            m_readingView->setScrollPositionVisualLine(s.scroll);
            // Drop saved folds when the document shape has shifted since
            // capture (line count delta). `setFoldedHeadingLines` is line-
            // indexed; carrying stale lines forward would fold the wrong
            // headings. Audit: editor-markdown.md §"Other" — fold-info
            // invalidates on line-count change.
            const QJsonValue savedCountVal = s.extraKeys.value(
                QStringLiteral("corbomite.foldedHeadingsLineCount"));
            const int currentCount = m_doc
                ? m_doc->markdown().count(QLatin1Char('\n')) + 1
                : 0;
            const bool shapeMatches = savedCountVal.isDouble()
                && savedCountVal.toInt() == currentCount;
            m_readingView->setFoldedHeadingLines(
                shapeMatches ? s.foldedHeadings : QVector<int>{});
        }
        break;
    }
}

void NoteEditorWidget::setViewMode(ViewMode newMode)
{
    if (m_viewMode == newMode) return;

    // 1. Capture outgoing leaf's ephemeral state (scroll/cursor/fold).
    const EphemeralState outgoing = captureEphemeralStateFor(m_viewMode);

    // 2. Detach outgoing leaf from the canonical document.
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

    // 5. Restore scroll + fold + cursor (mode-appropriate).
    restoreEphemeralStateFor(newMode, outgoing);

    // Mirror legacy behaviour: Reading mode makes Markoff read-only
    // (cosmetic, since Reading is a different widget now, but tests &
    // consumers might still inspect `m_editor->isReadOnly()`).
    m_editor->setReadOnly(newMode == ViewMode::Reading);

    Q_EMIT viewModeChanged(newMode);
}

NoteEditorWidget::ViewMode NoteEditorWidget::viewMode() const
{
    return m_viewMode;
}

Markoff::Editor *NoteEditorWidget::editor() const
{
    return m_editor;
}

Markoff::Source::Editor *NoteEditorWidget::sourceEditor() const
{
    return m_sourceEditor;
}

Markoff::Reading::ReadingView *NoteEditorWidget::readingView() const
{
    return m_readingView;
}

int NoteEditorWidget::currentLine() const
{
    return m_editor->cursorLine();
}

int NoteEditorWidget::currentColumn() const
{
    return m_editor->cursorColumn();
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
    if (!m_suggestManager || !m_doc) return;

    const int absPos = absoluteCursorPos();
    if (absPos < 0) {
        if (m_completionPopup) dismissCompletion();
        return;
    }
    const QString line = currentLineText();
    const int lineStart = absPos - m_editor->cursorColumn() + 1;
    const int colInLine = absPos - lineStart;

    auto result = m_suggestManager->dispatch(colInLine, line, m_doc);
    if (!result) {
        if (m_completionPopup) dismissCompletion();
        return;
    }

    // If a popup is already up for the same suggester + same trigger start,
    // just refilter; otherwise rebuild.
    const int absStart = lineStart + result->info.start;
    if (m_completionPopup && m_activeSuggester == result->suggester
        && m_completionTriggerPos == absStart) {
        updateCompletionFilter();
        return;
    }
    dismissCompletion();
    m_activeSuggester = result->suggester;
    m_completionTriggerPos = absStart;

    auto *model = new QStringListModel(result->suggester->getSuggestions(result->info), this);
    m_completionPopup = new CompletionPopup(model, m_editor->viewport());
    model->setParent(m_completionPopup);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() {
        m_completionPopup = nullptr;
        m_activeSuggester = nullptr;
        m_completionTriggerPos = -1;
    });
    m_completionPopup->setFilterText(result->info.query);
    positionCompletionPopup();
    m_completionPopup->show();
}

void NoteEditorWidget::positionCompletionPopup()
{
    if (!m_completionPopup) return;
    // cursorScreenRect is global; convert to viewport-local since
    // that's the popup's parent.
    QRect cr = m_editor->cursorScreenRect();
    QPoint local = m_editor->viewport()->mapFromGlobal(cr.bottomLeft());
    m_completionPopup->move(local + QPoint(0, 2));
}

int NoteEditorWidget::absoluteCursorPos() const
{
    int line = m_editor->cursorLine();
    int col = m_editor->cursorColumn();
    if (line < 1 || col < 1) return -1;
    QString src = m_editor->toPlainText();
    int currentLine = 1;
    for (int i = 0; i < src.size(); ++i) {
        if (currentLine == line) return i + col - 1;
        if (src[i] == QLatin1Char('\n')) ++currentLine;
    }
    return src.size();
}

QString NoteEditorWidget::currentLineText() const
{
    const int absPos = absoluteCursorPos();
    if (absPos < 0) return {};
    const QString src = m_editor->toPlainText();
    int start = absPos;
    while (start > 0 && src.at(start - 1) != QLatin1Char('\n')) --start;
    int end = absPos;
    while (end < src.size() && src.at(end) != QLatin1Char('\n')) ++end;
    return src.mid(start, end - start);
}

void NoteEditorWidget::updateCompletionFilter()
{
    if (!m_completionPopup) return;
    const int absPos = absoluteCursorPos();
    if (absPos < 0 || absPos < m_completionTriggerPos) {
        dismissCompletion();
        return;
    }
    const QString src = m_editor->toPlainText();
    QString filter = src.mid(m_completionTriggerPos,
                              absPos - m_completionTriggerPos);
    if (filter.contains(QLatin1Char('\n'))
        || filter.contains(QLatin1Char(']'))) {
        dismissCompletion();
        return;
    }
    m_completionPopup->setFilterText(filter);
    positionCompletionPopup();
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
    Q_UNUSED(data)

    const QString source = m_editor->toPlainText();
    const int triggerPos = m_completionTriggerPos;
    const int absPos = absoluteCursorPos();
    if (triggerPos < 0 || triggerPos > source.size() || absPos < 0
        || !m_activeSuggester || !m_doc) {
        dismissCompletion();
        return;
    }

    EditorSuggestTriggerInfo ctx;
    ctx.start = triggerPos;
    ctx.end = absPos;
    ctx.query = source.mid(triggerPos, absPos - triggerPos);
    const QString insertion = m_activeSuggester->selectSuggestion(text, ctx);

    // Phase C3: write the completion through the canonical MarkoffDocument
    // undo stack rather than bypassing it with setPlainText + setMarkdown.
    // This preserves undo history and keeps leaves in sync via their
    // contentsChanged subscriptions.
    const qsizetype removeLen = static_cast<qsizetype>(absPos - triggerPos);
    m_doc->markoff()->undoStack()->push(
        new Markoff::MarkdownDelta(m_doc->markoff(),
                                   static_cast<qsizetype>(triggerPos),
                                   removeLen,
                                   insertion));

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

} // namespace Corbomite
