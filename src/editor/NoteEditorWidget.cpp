// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "HoverPopover.h"
#include "SourceEditor.h"
#include "ViewModeSerializer.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/storage/EphemeralState.h"
#include "dialogs/QuickSwitcherModel.h"

#include <markoff/Editor.h>
#include <corbomite/readingview/ReadingView.h>

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
            this, [this](const QString &target) {
        if (!m_hoverPopover) return;
        if (target.isEmpty()) {
            m_hoverPopover->cancel();
        } else {
            // Anchor near the cursor; Markoff doesn't expose hovered-link
            // rect today (a Cluster H follow-up). The 20px y-offset keeps the
            // popover from sitting under the cursor and triggering leaveEvent.
            m_hoverPopover->scheduleShow(resolveTarget(target),
                                          QCursor::pos() + QPoint(0, 20));
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
    m_doc = doc;
    if (m_doc) {
        m_editor->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            m_editor->setResourceProvider(m_resourceProvider);
        }
        syncFromDocument();
    } else {
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

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

void NoteEditorWidget::setVaultModel(VaultModel *vault)
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
// Phase 7 — stacked-widget mode transition
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
            m_sourceEditor = new SourceEditor(this);
            m_sourceIndex = m_stack->addWidget(m_sourceEditor);
        }
        break;
    case ViewMode::LivePreview:
        // Always constructed eagerly in the ctor.
        break;
    case ViewMode::Reading:
        if (!m_readingView) {
            m_readingView = new Corbomite::ReadingView::ReadingView(this);
            m_readingIndex = m_stack->addWidget(m_readingView);
        }
        break;
    }
}

void NoteEditorWidget::saveSourceTextToDocument()
{
    if (!m_sourceEditor || !m_doc) return;
    const QString text = m_sourceEditor->toPlainText();
    // Only write through if the Source buffer has diverged from the document
    // — avoids needless `setModified(true)` during a mode flip that hasn't
    // actually edited anything.
    if (text != m_doc->markdown()) {
        m_doc->setMarkdown(text);
    }
}

void NoteEditorWidget::saveLivePreviewTextToDocument()
{
    if (!m_editor || !m_doc) return;
    const QString text = m_editor->toPlainText();
    if (text != m_doc->markdown()) {
        m_doc->setMarkdown(text);
    }
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
            const auto cursor = m_sourceEditor->cursorPosition();
            s.cursor.line = cursor.line;
            s.cursor.column = cursor.column;
            s.foldedHeadings = m_sourceEditor->foldedHeadings();
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
            s.foldedHeadings = m_readingView->foldedHeadings();
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
            m_sourceEditor->setCursorPosition({s.cursor.line, s.cursor.column});
            m_sourceEditor->setScrollPosition(s.scroll);
            m_sourceEditor->setFoldedHeadings(s.foldedHeadings);
        }
        break;
    case ViewMode::LivePreview:
        // Cursor — Markoff's public API has `goToLine` but no column setter.
        // Column preservation is best-effort: we land on the correct line;
        // the cursor-reveal-source behaviour handles the rest. Line is
        // 1-based on the wire, 0-based in EphemeralState.
        if (s.cursor.line > 0 || s.cursor.column > 0) {
            m_editor->goToLine(s.cursor.line + 1);
        }
        m_editor->setScrollPositionVisualLine(s.scroll);
        break;
    case ViewMode::Reading:
        if (m_readingView) {
            m_readingView->setScrollPositionVisualLine(s.scroll);
            m_readingView->setFoldedHeadings(s.foldedHeadings);
        }
        break;
    }
}

void NoteEditorWidget::loadContentInto(ViewMode mode)
{
    if (!m_doc) return;
    const QString markdown = m_doc->markdown();

    // Guard the doc-write-back path when we setPlainText on Markoff — it
    // emits textChanged which would otherwise re-enter `onTextChanged`.
    const bool wasUpdating = m_updatingFromDoc;
    m_updatingFromDoc = true;

    switch (mode) {
    case ViewMode::Source:
        if (m_sourceEditor && m_sourceEditor->toPlainText() != markdown) {
            m_sourceEditor->setPlainText(markdown);
        }
        break;
    case ViewMode::LivePreview:
        if (m_editor->toPlainText() != markdown) {
            m_editor->setPlainText(markdown);
        }
        break;
    case ViewMode::Reading:
        if (m_readingView) {
            m_readingView->setPlainText(markdown);
        }
        break;
    }

    m_updatingFromDoc = wasUpdating;
}

void NoteEditorWidget::setViewMode(ViewMode newMode)
{
    if (m_viewMode == newMode) return;

    // 1. Flush outgoing widget's text to the shared NoteDocument.
    //    Reading mode has no unflushed text so skip.
    if (m_viewMode == ViewMode::Source) {
        saveSourceTextToDocument();
    } else if (m_viewMode == ViewMode::LivePreview) {
        saveLivePreviewTextToDocument();
    }

    // 2. Capture the outgoing widget's ephemeral state.
    const EphemeralState outgoing = captureEphemeralStateFor(m_viewMode);

    // 3 + 4. Swap the active widget. Lazy-construct if needed.
    ensureWidgetConstructed(newMode);
    m_viewMode = newMode;
    const int idx = stackIndexFor(newMode);
    if (idx >= 0) m_stack->setCurrentIndex(idx);

    // 5. Load content into the incoming widget if not already in sync.
    loadContentInto(newMode);

    // 6. Restore scroll + fold + cursor (mode-appropriate).
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

SourceEditor *NoteEditorWidget::sourceEditor() const
{
    return m_sourceEditor;
}

Corbomite::ReadingView::ReadingView *NoteEditorWidget::readingView() const
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
    // Drive the full transition through setViewMode so the ensure/load/restore
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
    if (m_updatingFromDoc || !m_doc) return;
    m_doc->setMarkdown(m_editor->toPlainText());
    if (m_completionPopup) updateCompletionFilter();
}

void NoteEditorWidget::onCursorPositionChanged(int line, int column)
{
    if (!m_doc) return;
    Q_EMIT cursorInfoChanged(line, column, m_cachedWordCount);
}

void NoteEditorWidget::syncFromDocument()
{
    if (!m_doc) return;
    m_updatingFromDoc = true;
    const QString markdown = m_doc->markdown();
    // Seed the currently-visible mode's widget. Offscreen widgets get their
    // content lazily via `loadContentInto` on mode switch — avoids
    // instantiating Source/Reading just to populate them.
    switch (m_viewMode) {
    case ViewMode::Source:
        if (m_sourceEditor) m_sourceEditor->setPlainText(markdown);
        break;
    case ViewMode::LivePreview:
        m_editor->setPlainText(markdown);
        break;
    case ViewMode::Reading:
        if (m_readingView) m_readingView->setPlainText(markdown);
        break;
    }
    m_doc->setModified(false);
    m_updatingFromDoc = false;
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
        || !m_activeSuggester) {
        dismissCompletion();
        return;
    }

    EditorSuggestTriggerInfo ctx;
    ctx.start = triggerPos;
    ctx.end = absPos;
    ctx.query = source.mid(triggerPos, absPos - triggerPos);
    const QString insertion = m_activeSuggester->selectSuggestion(text, ctx);

    const QString before = source.left(triggerPos);
    const QString after = source.mid(absPos);

    m_updatingFromDoc = true;
    m_editor->setPlainText(before + insertion + after);
    m_updatingFromDoc = false;
    if (m_doc)
        m_doc->setMarkdown(m_editor->toPlainText());

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
