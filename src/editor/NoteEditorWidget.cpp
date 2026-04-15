// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "HoverPopover.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
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
    connect(m_editor, &Markoff::Editor::wikiLinkTrigger,
            this, &NoteEditorWidget::triggerWikiLinkCompletion);
    connect(m_editor, &Markoff::Editor::tagTrigger,
            this, &NoteEditorWidget::triggerTagCompletion);
    connect(m_editor, &Markoff::Editor::completionDismissHint,
            this, &NoteEditorWidget::dismissCompletion);

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

void NoteEditorWidget::triggerWikiLinkCompletion(int pos)
{
    if (!m_vault) return;
    dismissCompletion();
    m_completionTriggerPos = pos;

    m_completionMode = CompletionMode::WikiLink;

    auto *model = new QuickSwitcherModel(this);
    model->setNotes(m_vault->allNotes());

    // Parent the popup on the editor's viewport so it's a regular
    // child widget — NOT a top-level Qt::Popup. That keeps keystrokes
    // flowing into the editor.
    m_completionPopup = new CompletionPopup(model, m_editor->viewport());
    model->setParent(m_completionPopup);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() {
        m_completionPopup = nullptr;
        m_completionMode = CompletionMode::None;
        m_completionTriggerPos = -1;
    });

    positionCompletionPopup();
    m_completionPopup->show();
}

void NoteEditorWidget::triggerTagCompletion(int pos)
{
    if (!m_vault) return;
    dismissCompletion();
    m_completionTriggerPos = pos;

    m_completionMode = CompletionMode::Tag;

    auto *model = new QStringListModel(m_vault->allTags(), this);

    m_completionPopup = new CompletionPopup(model, m_editor->viewport());
    model->setParent(m_completionPopup);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() {
        m_completionPopup = nullptr;
        m_completionMode = CompletionMode::None;
        m_completionTriggerPos = -1;
    });

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

QString NoteEditorWidget::currentTriggerText() const
{
    if (m_completionTriggerPos < 0) return {};
    int absPos = absoluteCursorPos();
    QString src = m_editor->toPlainText();
    if (absPos < m_completionTriggerPos || absPos > src.size()) return {};
    return src.mid(m_completionTriggerPos, absPos - m_completionTriggerPos);
}

void NoteEditorWidget::updateCompletionFilter()
{
    if (!m_completionPopup) return;
    int absPos = absoluteCursorPos();
    if (absPos < 0 || absPos < m_completionTriggerPos) {
        dismissCompletion();
        return;
    }
    QString filter = currentTriggerText();
    // Bail if the user's typing crossed a newline or hit a closing
    // bracket — that means the trigger context is gone.
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
    m_completionMode = CompletionMode::None;
    m_completionTriggerPos = -1;
}

void NoteEditorWidget::onCompletionAccepted(const QString &text, const QString &data)
{
    Q_UNUSED(data)

    QString source = m_editor->toPlainText();
    int triggerPos = m_completionTriggerPos;
    if (triggerPos < 0 || triggerPos > source.size()) {
        dismissCompletion();
        return;
    }

    int line = m_editor->cursorLine();
    int col = m_editor->cursorColumn();
    if (line < 1 || col < 1) {
        dismissCompletion();
        return;
    }
    int absPos = 0;
    int currentLine = 1;
    bool found = false;
    for (int i = 0; i < source.size(); ++i) {
        if (currentLine == line) {
            absPos = i + col - 1;
            found = true;
            break;
        }
        if (source[i] == QLatin1Char('\n'))
            ++currentLine;
    }
    if (!found) {
        dismissCompletion();
        return;
    }

    QString before = source.left(triggerPos);
    QString after = source.mid(absPos);
    QString insertion = (m_completionMode == CompletionMode::WikiLink)
        ? text + QStringLiteral("]]")
        : text;

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
