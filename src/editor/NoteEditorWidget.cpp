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

#include <QCursor>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QStringListModel>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_editor(new Markoff::Editor(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editor);

    // Phase 2 mount — construct Corbomite::SourceEditor as a child and hide
    // it. This proves the build link + widget instantiation without
    // changing any user-visible behaviour. Cluster E Phase 7 will promote
    // it into the ViewMode switch.
    m_sourceEditor = new SourceEditor(this);
    m_sourceEditor->hide();

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

void NoteEditorWidget::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;

    // Phase 1 keeps the old visible behaviour: Reading toggles the Markoff
    // editor read-only, everything else is editable. Source mode is not yet
    // reachable through the UI — Phase 7 promotes `m_sourceEditor` into a
    // stacked-widget swap.
    m_editor->setReadOnly(mode == ViewMode::Reading);

    Q_EMIT viewModeChanged(mode);
}

NoteEditorWidget::ViewMode NoteEditorWidget::viewMode() const
{
    return m_viewMode;
}

Markoff::Editor *NoteEditorWidget::editor() const
{
    return m_editor;
}

int NoteEditorWidget::currentLine() const
{
    return m_editor->cursorLine();
}

int NoteEditorWidget::currentColumn() const
{
    return m_editor->cursorColumn();
}

// Cluster E Phase 1 — ephemeral state round-trip. Not yet called from the
// session-save path; that wiring is Phase 7. Keeping the capture/restore
// logic here now gives Phase 7 a stable surface to connect.
EphemeralState NoteEditorWidget::saveEphemeralState() const
{
    EphemeralState s;
    const auto compound = ViewModeSerializer::toCompound(m_viewMode);
    s.modeRaw = compound.mode;
    s.sourceFlag = compound.source;

    switch (m_viewMode) {
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
        // TODO(Cluster E Phase 2): Markoff lacks a visual-line float scroll
        // accessor today. Using 0.0 as a placeholder; cursor line/column
        // are available through the existing accessors.
        s.scroll = 0.0f;
        s.cursor.line = m_editor->cursorLine();
        s.cursor.column = m_editor->cursorColumn();
        break;
    case ViewMode::Reading:
        // Reading today is still `setReadOnly(true)` on Markoff (per the plan's
        // "Reality reconciliation"). No scroll/fold accessor; the Phase 3
        // ReadingView widget will fill these.
        s.scroll = 0.0f;
        break;
    }

    return s;
}

void NoteEditorWidget::restoreEphemeralState(const EphemeralState &state)
{
    const auto mode = ViewModeSerializer::fromCompound(state.modeRaw,
                                                       std::optional<bool>{state.sourceFlag});
    setViewMode(mode);

    switch (mode) {
    case ViewMode::Source:
        if (m_sourceEditor) {
            m_sourceEditor->setCursorPosition({state.cursor.line, state.cursor.column});
            m_sourceEditor->setScrollPosition(state.scroll);
            m_sourceEditor->setFoldedHeadings(state.foldedHeadings);
        }
        break;
    case ViewMode::LivePreview:
        // TODO(Cluster E Phase 2): no visual-line float scroll on Markoff
        // yet. Cursor restore is similarly deferred — Markoff's
        // setCursorPosition lands alongside ScrollPosition.
        break;
    case ViewMode::Reading:
        // Reading widget is the stub `setReadOnly(true)` Markoff for now;
        // Phase 3 builds the real ReadingView, Phase 7 wires restore.
        break;
    }
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
    m_editor->setPlainText(m_doc->markdown());
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
