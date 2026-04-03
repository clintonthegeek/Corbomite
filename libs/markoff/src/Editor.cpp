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

    // Performance: disable BSP indexing (linear scan faster for <20 items),
    // skip painter state save/restore, avoid viewport update coalescing issues.
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setCacheMode(QGraphicsView::CacheNone);

    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setCursor(Qt::IBeamCursor);

    verticalScrollBar()->setSingleStep(20);

    // Auto-scroll timer for drag selection outside viewport
    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(50);
    connect(m_autoScrollTimer, &QTimer::timeout, this, &Editor::doAutoScroll);

    connect(m_coordinator, &SceneCoordinator::textChanged,
            this, &Editor::textChanged);

    // Ensure cursor visible after text changes (typing, paste, etc.)
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
        return; // scrollbar didn't move (at min/max)

    // Synthesize a mouse move at the viewport edge. This goes through
    // Qt's normal scene event path: within-item selection grows via the
    // grabbed TextControl, and the SelectionManager takes over when
    // the scene position exits the item boundary.
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
    bool hasCrossBoundary = mgr->hasSelection();

    // Undo/Redo — delegate to focused text item
    QAction *undoAction = menu.addAction(tr("Undo"), [this]() {
        if (auto *item = m_scene->focusItem()) {
            if (auto *textItem = dynamic_cast<MarkdownTextItem *>(item))
                textItem->textControl()->undo();
        }
    });
    QAction *redoAction = menu.addAction(tr("Redo"), [this]() {
        if (auto *item = m_scene->focusItem()) {
            if (auto *textItem = dynamic_cast<MarkdownTextItem *>(item))
                textItem->textControl()->redo();
        }
    });
    undoAction->setShortcut(QKeySequence::Undo);
    redoAction->setShortcut(QKeySequence::Redo);

    menu.addSeparator();

    // Cut
    QAction *cutAction = menu.addAction(tr("Cut"), [this, mgr]() {
        if (mgr->hasSelection()) {
            QMimeData *data = mgr->createMimeData();
            QApplication::clipboard()->setMimeData(data);
            // TODO: delete selected content
        } else if (auto *item = m_scene->focusItem()) {
            if (auto *textItem = dynamic_cast<MarkdownTextItem *>(item))
                textItem->textControl()->cut();
        }
    });
    cutAction->setShortcut(QKeySequence::Cut);

    // Copy
    QAction *copyAction = menu.addAction(tr("Copy"), [this, mgr]() {
        if (mgr->hasSelection()) {
            QMimeData *data = mgr->createMimeData();
            QApplication::clipboard()->setMimeData(data);
        } else if (auto *item = m_scene->focusItem()) {
            if (auto *textItem = dynamic_cast<MarkdownTextItem *>(item))
                textItem->textControl()->copy();
        }
    });
    copyAction->setShortcut(QKeySequence::Copy);

    // Paste
    QAction *pasteAction = menu.addAction(tr("Paste"), [this]() {
        if (auto *item = m_scene->focusItem()) {
            if (auto *textItem = dynamic_cast<MarkdownTextItem *>(item))
                textItem->textControl()->paste();
        }
    });
    pasteAction->setShortcut(QKeySequence::Paste);

    menu.addSeparator();

    // Select All
    menu.addAction(tr("Select All"), [mgr]() {
        QKeyEvent selectAll(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        mgr->handleKeyPress(&selectAll);
    });

    // Enable/disable based on state
    bool hasSelection = hasCrossBoundary;
    if (!hasSelection) {
        if (auto *item = m_scene->focusItem()) {
            if (auto *textItem = dynamic_cast<MarkdownTextItem *>(item))
                hasSelection = textItem->textControl()->textCursor().hasSelection();
        }
    }
    cutAction->setEnabled(hasSelection && m_mode != Mode::Source);
    copyAction->setEnabled(hasSelection);

    bool canPaste = QApplication::clipboard()->mimeData()
        && QApplication::clipboard()->mimeData()->hasText();
    pasteAction->setEnabled(canPaste);

    menu.exec(e->globalPos());
}

// =========================================================================
// Keyboard enhancements
// =========================================================================

void Editor::keyPressEvent(QKeyEvent *e)
{
    // Ctrl+Home: jump to document start
    if (e->key() == Qt::Key_Home && e->modifiers() == Qt::ControlModifier) {
        const auto &items = m_coordinator->items();
        for (auto *item : items) {
            if (item->isTextItem()) {
                auto *textItem = static_cast<MarkdownTextItem *>(item);
                textItem->setFocus();
                QTextCursor cursor(textItem->document());
                cursor.movePosition(QTextCursor::Start);
                textItem->textControl()->setTextCursor(cursor);
                ensureFocusedCursorVisible();
                return;
            }
        }
    }

    // Ctrl+Plus/Minus: zoom
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

    // Ctrl+End: jump to document end
    if (e->key() == Qt::Key_End && e->modifiers() == Qt::ControlModifier) {
        const auto &items = m_coordinator->items();
        for (int i = items.size() - 1; i >= 0; --i) {
            if (items[i]->isTextItem()) {
                auto *textItem = static_cast<MarkdownTextItem *>(items[i]);
                textItem->setFocus();
                QTextCursor cursor(textItem->document());
                cursor.movePosition(QTextCursor::End);
                textItem->textControl()->setTextCursor(cursor);
                ensureFocusedCursorVisible();
                return;
            }
        }
    }

    QGraphicsView::keyPressEvent(e);

    // Ensure cursor is visible after any key press that might move it
    if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down
        || e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown
        || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter
        || e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete) {
        ensureFocusedCursorVisible();
    }
}

// =========================================================================
// Zoom (Ctrl+scroll, Ctrl+Plus/Minus)
// =========================================================================

void Editor::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        // Zoom
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
    // Don't fight auto-scroll — it manages viewport position itself
    if (m_autoScrollActive)
        return;

    auto *focusItem = m_scene->focusItem();
    if (!focusItem) return;

    auto *textItem = dynamic_cast<MarkdownTextItem *>(focusItem);
    if (!textItem) return;

    QRectF cursorRect = textItem->textControl()->cursorRect();
    QRectF sceneRect = textItem->mapToScene(cursorRect).boundingRect();
    ensureVisible(sceneRect, 0, 50);
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
