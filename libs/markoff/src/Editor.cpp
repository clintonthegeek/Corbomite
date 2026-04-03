// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Editor.h"
#include "SelectionScene.h"
#include "SelectionManager.h"
#include "SceneCoordinator.h"
#include "SelectableItem.h"
#include "MarkdownTextItem.h"
#include "TextControl.h"

#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QScrollBar>
#include <QTextDocument>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>

namespace Markoff {

Editor::Editor(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new SelectionScene(this))
    , m_coordinator(new SceneCoordinator(m_scene, this))
{
    setScene(m_scene);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setDragMode(QGraphicsView::NoDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setCacheMode(QGraphicsView::CacheNone);

    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setCursor(Qt::IBeamCursor);

    verticalScrollBar()->setSingleStep(20);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(50);
    connect(m_autoScrollTimer, &QTimer::timeout, this, &Editor::doAutoScroll);

    connect(m_coordinator, &SceneCoordinator::textChanged,
            this, &Editor::textChanged);
    connect(m_coordinator, &SceneCoordinator::textChanged,
            this, &Editor::ensureFocusedCursorVisible);
}

Editor::~Editor() = default;

void Editor::setPlainText(const QString &text)
{
    m_sourceText = text;
    rebuildScene();
}

QString Editor::toPlainText() const
{
    if (m_mode == Mode::Source) {
        if (!m_coordinator->items().isEmpty() && m_coordinator->items().first()->isTextItem())
            return m_coordinator->items().first()->allMarkdown();
        return m_sourceText;
    }
    return m_coordinator->toMarkdown();
}

void Editor::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_sourceText = toPlainText();
    m_mode = mode;
    rebuildScene();
}

void Editor::setFontSize(int pointSize)
{
    m_fontSize = pointSize;
    QFont font = this->font();
    font.setPointSize(pointSize);
    m_coordinator->setFont(font);
}

void Editor::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    qreal width = viewport()->width() - 32;
    if (width > 100)
        m_coordinator->setItemWidth(width);
}

// =========================================================================
// Auto-scroll during drag selection
// =========================================================================

void Editor::mouseMoveEvent(QMouseEvent *e)
{
    QGraphicsView::mouseMoveEvent(e);

    if (e->buttons() & Qt::LeftButton) {
        int y = e->pos().y();
        if (y < 0)
            startAutoScroll(y);
        else if (y > viewport()->height())
            startAutoScroll(y - viewport()->height());
        else
            stopAutoScroll();
    }
}

void Editor::mouseReleaseEvent(QMouseEvent *e)
{
    QGraphicsView::mouseReleaseEvent(e);
    stopAutoScroll();
}

void Editor::startAutoScroll(int delta)
{
    m_autoScrollDelta = qBound(-60, delta, 60);
    m_autoScrollActive = true;
    if (!m_autoScrollTimer->isActive())
        m_autoScrollTimer->start();
}

void Editor::stopAutoScroll()
{
    m_autoScrollTimer->stop();
    m_autoScrollDelta = 0;
    m_autoScrollActive = false;
}

void Editor::doAutoScroll()
{
    QScrollBar *vbar = verticalScrollBar();
    int oldVal = vbar->value();
    vbar->setValue(oldVal + m_autoScrollDelta);
    if (vbar->value() == oldVal)
        return;

    QPoint viewportEdge;
    if (m_autoScrollDelta < 0)
        viewportEdge = QPoint(viewport()->width() / 2, 0);
    else
        viewportEdge = QPoint(viewport()->width() / 2, viewport()->height() - 1);

    QPointF scenePos = mapToScene(viewportEdge);
    QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
    moveEvent.setScenePos(scenePos);
    moveEvent.setScreenPos(mapToGlobal(viewportEdge));
    moveEvent.setButtons(Qt::LeftButton);
    moveEvent.setButton(Qt::NoButton);
    moveEvent.setModifiers(QApplication::keyboardModifiers());
    QApplication::sendEvent(m_scene, &moveEvent);
}

// =========================================================================
// Context menu
// =========================================================================

void Editor::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu menu(this);
    auto *mgr = m_scene->selectionManager();

    auto *undoAction = menu.addAction(tr("Undo"), this, [this]() {
        if (auto *ti = focusedTextItem()) ti->textControl()->undo();
    });
    auto *redoAction = menu.addAction(tr("Redo"), this, [this]() {
        if (auto *ti = focusedTextItem()) ti->textControl()->redo();
    });
    undoAction->setShortcut(QKeySequence::Undo);
    redoAction->setShortcut(QKeySequence::Redo);

    menu.addSeparator();

    auto *cutAction = menu.addAction(tr("Cut"), this, [this, mgr]() {
        if (mgr->hasSelection()) {
            QApplication::clipboard()->setMimeData(mgr->createMimeData());
        } else if (auto *ti = focusedTextItem()) {
            ti->textControl()->cut();
        }
    });
    auto *copyAction = menu.addAction(tr("Copy"), this, [this, mgr]() {
        if (mgr->hasSelection()) {
            QApplication::clipboard()->setMimeData(mgr->createMimeData());
        } else if (auto *ti = focusedTextItem()) {
            ti->textControl()->copy();
        }
    });
    auto *pasteAction = menu.addAction(tr("Paste"), this, [this]() {
        if (auto *ti = focusedTextItem()) ti->textControl()->paste();
    });

    menu.addSeparator();

    menu.addAction(tr("Select All"), this, [mgr]() {
        QKeyEvent e(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        mgr->handleKeyPress(&e);
    });

    // Enable/disable
    bool hasSelection = mgr->hasSelection();
    if (!hasSelection) {
        if (auto *ti = focusedTextItem())
            hasSelection = ti->textControl()->textCursor().hasSelection();
    }
    cutAction->setEnabled(hasSelection);
    copyAction->setEnabled(hasSelection);
    pasteAction->setEnabled(QApplication::clipboard()->mimeData()
                            && QApplication::clipboard()->mimeData()->hasText());

    menu.exec(e->globalPos());
}

// =========================================================================
// Keyboard
// =========================================================================

void Editor::keyPressEvent(QKeyEvent *e)
{
    // Zoom: Ctrl+Plus/Minus
    if (e->modifiers() & Qt::ControlModifier) {
        if (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal) {
            if (m_fontSize < 48) setFontSize(m_fontSize + 1);
            return;
        }
        if (e->key() == Qt::Key_Minus) {
            if (m_fontSize > 6) setFontSize(m_fontSize - 1);
            return;
        }
    }

    bool shift = e->modifiers() & Qt::ShiftModifier;
    bool ctrl = e->modifiers() & Qt::ControlModifier;

    // Ctrl+Home / Ctrl+Shift+Home: jump/select to document start
    if (e->key() == Qt::Key_Home && ctrl) {
        jumpToDocumentEdge(true, shift);
        return;
    }
    // Ctrl+End / Ctrl+Shift+End: jump/select to document end
    if (e->key() == Qt::Key_End && ctrl) {
        jumpToDocumentEdge(false, shift);
        return;
    }

    // Page Up/Down: scroll by viewport height, move cursor
    if (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown) {
        pageUpDown(e->key() == Qt::Key_PageUp, shift);
        return;
    }

    QGraphicsView::keyPressEvent(e);

    // Ensure cursor visible after cursor-moving keys
    switch (e->key()) {
    case Qt::Key_Up: case Qt::Key_Down:
    case Qt::Key_Return: case Qt::Key_Enter:
    case Qt::Key_Backspace: case Qt::Key_Delete:
        ensureFocusedCursorVisible();
        break;
    default:
        // For regular typing, also ensure visible
        if (!e->text().isEmpty())
            ensureFocusedCursorVisible();
        break;
    }
}

void Editor::jumpToDocumentEdge(bool toStart, bool select)
{
    const auto &items = m_coordinator->items();
    if (items.isEmpty()) return;

    // If selecting across entire document, use SelectionManager
    if (select) {
        auto *mgr = m_scene->selectionManager();
        QKeyEvent e(QEvent::KeyPress,
                    toStart ? Qt::Key_Home : Qt::Key_End,
                    Qt::ControlModifier | Qt::ShiftModifier);
        // Ctrl+A handles this — select everything
        QKeyEvent selectAll(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        mgr->handleKeyPress(&selectAll);
        ensureFocusedCursorVisible();
        return;
    }

    // Jump without selection
    if (toStart) {
        for (auto *item : items) {
            if (item->isTextItem()) {
                auto *ti = static_cast<MarkdownTextItem *>(item);
                ti->setFocus();
                QTextCursor c(ti->document());
                c.movePosition(QTextCursor::Start);
                ti->textControl()->setTextCursor(c);
                break;
            }
        }
    } else {
        for (int i = items.size() - 1; i >= 0; --i) {
            if (items[i]->isTextItem()) {
                auto *ti = static_cast<MarkdownTextItem *>(items[i]);
                ti->setFocus();
                QTextCursor c(ti->document());
                c.movePosition(QTextCursor::End);
                ti->textControl()->setTextCursor(c);
                break;
            }
        }
    }
    ensureFocusedCursorVisible();
}

void Editor::pageUpDown(bool up, bool select)
{
    auto *ti = focusedTextItem();
    if (!ti) return;

    // Scroll by viewport height (following QPlainTextEdit pattern)
    QScrollBar *vbar = verticalScrollBar();
    int pageStep = viewport()->height();
    if (up)
        vbar->setValue(vbar->value() - pageStep);
    else
        vbar->setValue(vbar->value() + pageStep);

    // Move cursor to visible area. Map the viewport center to scene
    // coords and find the nearest text item at that position.
    QPointF sceneCenter = mapToScene(viewport()->width() / 2,
                                      viewport()->height() / 2);
    // Find which text item is near this position
    for (auto *item : m_coordinator->items()) {
        if (!item->isTextItem()) continue;
        auto *textItem = static_cast<MarkdownTextItem *>(item);
        QRectF sceneBounds = textItem->asGraphicsItem()->sceneBoundingRect();
        if (sceneBounds.contains(sceneCenter) ||
            (up && sceneBounds.bottom() >= sceneCenter.y()) ||
            (!up && sceneBounds.top() <= sceneCenter.y())) {
            textItem->setFocus();
            QPointF localPos = textItem->mapFromScene(sceneCenter);
            int pos = textItem->document()->documentLayout()->hitTest(localPos, Qt::FuzzyHit);
            if (pos >= 0) {
                QTextCursor cursor = textItem->textControl()->textCursor();
                if (select)
                    cursor.setPosition(pos, QTextCursor::KeepAnchor);
                else
                    cursor.setPosition(pos);
                textItem->textControl()->setTextCursor(cursor);
            }
            break;
        }
    }
}

// =========================================================================
// Zoom
// =========================================================================

void Editor::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        int delta = e->angleDelta().y();
        if (delta > 0 && m_fontSize < 48)
            setFontSize(m_fontSize + 1);
        else if (delta < 0 && m_fontSize > 6)
            setFontSize(m_fontSize - 1);
        e->accept();
        return;
    }
    QGraphicsView::wheelEvent(e);
}

// =========================================================================
// Ensure cursor visible
// =========================================================================

void Editor::ensureFocusedCursorVisible()
{
    if (m_autoScrollActive)
        return;

    auto *ti = focusedTextItem();
    if (!ti) return;

    QRectF cursorRect = ti->textControl()->cursorRect();
    QRectF sceneRect = ti->mapToScene(cursorRect).boundingRect();
    ensureVisible(sceneRect, 0, 50);
}

MarkdownTextItem *Editor::focusedTextItem() const
{
    auto *item = m_scene->focusItem();
    if (!item) return nullptr;
    return dynamic_cast<MarkdownTextItem *>(item);
}

// =========================================================================
// Scene rebuild
// =========================================================================

void Editor::rebuildScene()
{
    if (m_mode == Mode::Source)
        m_coordinator->loadSource(m_sourceText);
    else
        m_coordinator->loadMarkdown(m_sourceText);

    qreal width = viewport()->width() - 32;
    if (width > 100)
        m_coordinator->setItemWidth(width);
    if (m_fontSize > 0) {
        QFont font = this->font();
        font.setPointSize(m_fontSize);
        m_coordinator->setFont(font);
    }

    for (auto *item : m_coordinator->items()) {
        if (item->isTextItem()) {
            item->asGraphicsItem()->setFocus();
            break;
        }
    }
}

} // namespace Markoff
