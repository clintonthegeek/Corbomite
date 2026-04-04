// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
#include "dialogs/QuickSwitcherModel.h"

#include <markoff/Editor.h>
#include <markoff/ReadingView.h>

#include <QKeyEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QStringListModel>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_modeStack(new QStackedWidget(this))
    , m_editor(new Markoff::Editor(m_modeStack))
    , m_readingView(new Markoff::ReadingView(m_modeStack))
{
    m_modeStack->addWidget(m_editor);      // index 0: Source / LivePreview
    m_modeStack->addWidget(m_readingView); // index 1: Reading
    m_modeStack->setCurrentIndex(0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_modeStack);

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
    connect(m_editor, &Markoff::Editor::wikiLinkTrigger,
            this, [this](int pos) {
        m_completionTriggerPos = pos;
        triggerWikiLinkCompletion();
    });
    connect(m_editor, &Markoff::Editor::tagTrigger,
            this, [this](int pos) {
        m_completionTriggerPos = pos;
        triggerTagCompletion();
    });
    connect(m_editor, &Markoff::Editor::completionDismissHint,
            this, &NoteEditorWidget::dismissCompletion);
    connect(m_readingView, &Markoff::ReadingView::linkClicked,
            this, [this](const QString &target) {
        Q_EMIT linkActivated(resolveTarget(target));
    });

    m_editor->installEventFilter(this);
}

void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    m_doc = doc;
    if (m_doc) {
        m_editor->setResourceProvider(nullptr);
        m_readingView->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            m_editor->setResourceProvider(m_resourceProvider);
            m_readingView->setResourceProvider(m_resourceProvider);
        }
        syncFromDocument();
        if (m_viewMode == ViewMode::Reading)
            m_readingView->setMarkdown(m_doc->markdown());
    } else {
        m_editor->clear();
        m_readingView->setMarkdown({});
    }
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
        m_readingView->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
        m_editor->setResourceProvider(m_resourceProvider);
        m_readingView->setResourceProvider(m_resourceProvider);
    }
}

void NoteEditorWidget::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;

    if (mode == ViewMode::Reading) {
        if (m_doc)
            m_readingView->setMarkdown(m_editor->toPlainText());
        m_modeStack->setCurrentWidget(m_readingView);
    } else {
        m_editor->setMode(mode == ViewMode::LivePreview
            ? Markoff::Editor::Mode::LivePreview
            : Markoff::Editor::Mode::Source);
        m_modeStack->setCurrentWidget(m_editor);
    }

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
    if (obj == m_editor && event->type() == QEvent::KeyPress && m_completionPopup) {
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
            if (m_completionPopup->hasSelection()) {
                onCompletionAccepted(m_completionPopup->selectedText(),
                                     m_completionPopup->selectedData());
                return true;
            }
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

void NoteEditorWidget::triggerWikiLinkCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::WikiLink;

    auto *model = new QuickSwitcherModel(this);
    model->setNotes(m_vault->allNotes());

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() {
        m_completionPopup = nullptr;
        m_completionMode = CompletionMode::None;
    });

    QRect cr = m_editor->cursorScreenRect();
    m_completionPopup->move(cr.bottomLeft() + QPoint(0, 2));
    m_completionPopup->show();
}

void NoteEditorWidget::triggerTagCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::Tag;

    auto *model = new QStringListModel(m_vault->allTags(), this);

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() {
        m_completionPopup = nullptr;
        m_completionMode = CompletionMode::None;
    });

    QRect cr = m_editor->cursorScreenRect();
    m_completionPopup->move(cr.bottomLeft() + QPoint(0, 2));
    m_completionPopup->show();
}

void NoteEditorWidget::dismissCompletion()
{
    if (m_completionPopup) {
        m_completionPopup->close();
        m_completionPopup = nullptr;
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
