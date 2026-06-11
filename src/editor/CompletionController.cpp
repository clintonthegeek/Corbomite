// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionController.h"

#include "CompletionPopup.h"
#include "LineResolve.h"
#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/BlockEdit.h>
#include <markoff/core/MarkoffDocument.h>

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QTimer>

namespace Corbomite {

CompletionController::CompletionController(QObject *parent)
    : QObject(parent)
    , m_model(new QStandardItemModel(this))
{
}

CompletionController::~CompletionController()
{
    dismiss();
}

void CompletionController::setManager(EditorSuggestManager *manager)
{
    m_manager = manager;
}

void CompletionController::setLeaf(Markoff::MarkdownView *leaf)
{
    if (m_leaf == leaf) return;
    dismiss();
    if (m_leafCursorCon) QObject::disconnect(m_leafCursorCon);
    m_leaf = leaf;
    if (m_leaf) {
        m_leafCursorCon = connect(m_leaf, &Markoff::MarkdownView::cursorPositionChanged,
                                  this, [this](int, int) { scheduleRefresh(); });
    }
}

void CompletionController::setNoteDocument(NoteDocument *doc)
{
    if (m_doc == doc) return;
    dismiss();
    if (m_docChangedCon)   QObject::disconnect(m_docChangedCon);
    if (m_docDestroyedCon) QObject::disconnect(m_docDestroyedCon);
    m_doc = doc;
    if (m_doc && m_doc->markoff()) {
        m_docChangedCon = connect(m_doc->markoff(), &Markoff::MarkoffDocument::d2DocumentChanged,
                                  this, [this] { scheduleRefresh(); });
        m_docDestroyedCon = connect(m_doc->markoff(), &QObject::destroyed, this, [this] {
            m_doc = nullptr;
            dismiss();
        });
    }
}

bool CompletionController::isActive() const
{
    return m_popup && m_popup->isVisible();
}

void CompletionController::dismiss()
{
    if (!m_popup) return;
    CompletionPopup *p = m_popup;
    m_popup = nullptr;                  // null first: hideEvent→dismissed→dismiss() recursion guard
    qApp->removeEventFilter(this);
    p->hide();
    p->deleteLater();
}

void CompletionController::scheduleRefresh()
{
    if (m_refreshPending) return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, &CompletionController::refresh);
}

void CompletionController::refresh()
{
    m_refreshPending = false;
    if (!m_manager || !m_leaf || !m_doc || !m_doc->markoff() || !m_leaf->hasEditing()) {
        dismiss();
        return;
    }
    const QRect caret = m_leaf->caretRect();
    if (!caret.isValid()) {
        dismiss();
        return;
    }
    const Markoff::CursorPos pos = m_leaf->cursorPosition();
    const auto line = LineResolve::resolveLine(m_doc->markoff(), pos.line);
    if (!line) { dismiss(); return; }
    auto res = m_manager->dispatch(pos.column - 1, line->lineText, m_doc);
    if (!res) { dismiss(); return; }
    const EditorSuggestionSet set = res->suggester->getSuggestions(res->info);
    if (set.items.isEmpty()) { dismiss(); return; }

    ensurePopup();
    m_model->clear();
    for (const auto &it : set.items) {
        auto *item = new QStandardItem(it.display);
        item->setData(it.insertText, Qt::UserRole + 1);
        item->setData(it.detail, Qt::UserRole + 2);
        item->setEditable(false);
        m_model->appendRow(item);
    }
    m_popup->setFilterText(set.filter);
    if (m_popup->visibleRowCount() == 0) { dismiss(); return; }
    positionPopup(caret);
    m_popup->show();
}

void CompletionController::ensurePopup()
{
    if (m_popup) return;
    m_popup = new CompletionPopup(m_model, m_leaf);
    connect(m_popup, &CompletionPopup::itemSelected, this, &CompletionController::accept);
    connect(m_popup, &CompletionPopup::dismissed, this, &CompletionController::dismiss);
    // Scoped key interception: only while a popup lives. Installed AFTER
    // ScopeManager's app-wide filter, so ours runs FIRST (Qt runs
    // later-installed application filters first).
    qApp->installEventFilter(this);
}

void CompletionController::positionPopup(const QRect &caret)
{
    QPoint anchor = caret.bottomLeft() + QPoint(0, 2);
    const QSize hint = m_popup->sizeHint();
    if (anchor.y() + hint.height() > m_leaf->height()
        && caret.top() - hint.height() - 2 >= 0) {
        anchor = caret.topLeft() - QPoint(0, hint.height() + 2);
    }
    anchor.setX(qBound(0, anchor.x(), qMax(0, m_leaf->width() - hint.width())));
    m_popup->move(anchor);
}

bool CompletionController::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_popup) return false;
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Down:   m_popup->selectNext();     return true;
        case Qt::Key_Up:     m_popup->selectPrevious(); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_popup->acceptCurrent()) return true;
            break;
        case Qt::Key_Escape: dismiss();                 return true;
        default: break;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto *w = qobject_cast<QWidget *>(obj);
        if (w && m_leaf && !m_popup->isAncestorOf(w) && w != m_popup
            && !m_leaf->isAncestorOf(w) && w != m_leaf) {
            dismiss();
        }
    } else if (event->type() == QEvent::ApplicationDeactivate) {
        dismiss();
    }
    return false;
}

void CompletionController::accept(const QString &display, const QString &insertText)
{
    Q_UNUSED(display)
    // Re-resolve from the live snapshot — the popup's view of the world may
    // be one coalesced refresh stale. If the trigger no longer holds, abort
    // silently (spec §6): never insert against a guessed range.
    if (!m_manager || !m_leaf || !m_doc || !m_doc->markoff() || insertText.isEmpty()) {
        dismiss();
        return;
    }
    Markoff::MarkoffDocument *mdoc = m_doc->markoff();
    const Markoff::CursorPos pos = m_leaf->cursorPosition();
    const auto line = LineResolve::resolveLine(mdoc, pos.line);
    if (!line) { dismiss(); return; }
    auto res = m_manager->dispatch(pos.column - 1, line->lineText, m_doc);
    if (!res) { dismiss(); return; }
    const auto &info = res->info;
    const int replaceEnd = (info.replaceEnd >= info.end) ? info.replaceEnd : info.end;

    const QString blockStr = QString::fromUtf8(mdoc->blockText(line->blockId));
    const uint32_t b0 = LineResolve::byteOffsetForChar(
        blockStr, line->lineStartCharInBlock + info.start);
    const uint32_t b1 = LineResolve::byteOffsetForChar(
        blockStr, line->lineStartCharInBlock + replaceEnd);

    Markoff::BlockEdit edit;
    edit.blockId = line->blockId;
    edit.withinBlockByteOffset = b0;
    edit.removedBytes = b1 - b0;
    edit.insertedUtf8 = insertText.toUtf8();
    mdoc->applyBlockEdit(edit);          // undo-integrated; propagates to all leaves

    // Deterministic post-insert caret — never rely on a leaf's own
    // post-edit cursor behavior (spec §5).
    m_leaf->setCursorPosition({pos.line, info.start + int(insertText.length()) + 1});
    dismiss();
}

} // namespace Corbomite
