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
#include <limits>
#include <memory>

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
    connect(m_coordinator, &SceneCoordinator::textChanged,
            this, &Editor::onDocumentReparsed);
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
    Q_EMIT modeChanged(mode);
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

    detectCompletionTriggers(e->text());
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
    auto *sourceItem = focusedTextItem();
    if (!sourceItem) return;

    // Remember anchor for selection before scrolling
    int anchorPos = sourceItem->textControl()->textCursor().anchor();

    // Scroll by viewport height
    QScrollBar *vbar = verticalScrollBar();
    int pageStep = viewport()->height();
    vbar->setValue(vbar->value() + (up ? -pageStep : pageStep));

    // Find the text item nearest to viewport center (same nearest-by-Y
    // logic as SelectionManager::itemAt).
    QPointF sceneTarget = mapToScene(viewport()->width() / 2,
                                      viewport()->height() / 2);
    MarkdownTextItem *targetItem = nullptr;
    int targetPos = -1;
    qreal bestDist = std::numeric_limits<qreal>::max();

    for (auto *item : m_coordinator->items()) {
        if (!item->isTextItem()) continue;
        QRectF r = item->asGraphicsItem()->sceneBoundingRect();
        qreal dist = 0;
        if (sceneTarget.y() < r.top())
            dist = r.top() - sceneTarget.y();
        else if (sceneTarget.y() > r.bottom())
            dist = sceneTarget.y() - r.bottom();
        if (dist < bestDist) {
            bestDist = dist;
            targetItem = static_cast<MarkdownTextItem *>(item);
        }
    }
    if (!targetItem) return;

    QPointF localPos = targetItem->mapFromScene(sceneTarget);
    targetPos = targetItem->document()->documentLayout()->hitTest(localPos, Qt::FuzzyHit);
    if (targetPos < 0) targetPos = 0;

    if (!select) {
        // Simple navigation — clear any selection, move cursor
        m_scene->selectionManager()->clearSelection();
        targetItem->setFocus();
        QTextCursor cursor(targetItem->document());
        cursor.setPosition(targetPos);
        targetItem->textControl()->setTextCursor(cursor);
    } else if (targetItem == sourceItem) {
        // Same item — extend within-item selection
        QTextCursor cursor = sourceItem->textControl()->textCursor();
        cursor.setPosition(targetPos, QTextCursor::KeepAnchor);
        sourceItem->textControl()->setTextCursor(cursor);
    } else {
        // Cross-item — use SelectionManager
        auto *mgr = m_scene->selectionManager();

        // Set anchor item's selection from anchor to its edge
        int edgePos = up ? 0 : sourceItem->allMarkdown().length();
        sourceItem->setSelection(anchorPos, edgePos);

        // Fully select all items between source and target
        const auto &items = m_coordinator->items();
        int srcIdx = items.indexOf(static_cast<SelectableItem *>(sourceItem));
        int tgtIdx = items.indexOf(static_cast<SelectableItem *>(targetItem));
        int lo = qMin(srcIdx, tgtIdx), hi = qMax(srcIdx, tgtIdx);
        for (int i = lo + 1; i < hi; ++i) {
            if (items[i]->isTextItem())
                items[i]->setSelection(0, items[i]->allMarkdown().length());
            else
                items[i]->setFullySelected(true);
        }

        // Move focus and caret to target
        targetItem->setFocus();
        QTextCursor cursor(targetItem->document());
        int entryPos = up ? targetItem->allMarkdown().length() : 0;
        cursor.setPosition(entryPos);
        cursor.setPosition(targetPos, QTextCursor::KeepAnchor);
        targetItem->textControl()->setTextCursor(cursor);

        mgr->beginOrExtendKeyboardSelection(
            sourceItem, anchorPos, targetItem, targetPos);
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

// =========================================================================
// Configuration
// =========================================================================

void Editor::setTheme(const Theme &theme)
{
    m_theme = theme;
    m_fontSize = theme.textFont.pointSize() > 0 ? theme.textFont.pointSize() : 14;
    if (m_coordinator)
        m_coordinator->setTheme(theme);
}

Theme Editor::theme() const { return m_theme; }

void Editor::setEditorSettings(const EditorSettings &settings)
{
    m_editorSettings = settings;
}

EditorSettings Editor::editorSettings() const { return m_editorSettings; }

void Editor::setRenderSettings(const RenderSettings &settings)
{
    m_renderSettings = settings;
}

RenderSettings Editor::renderSettings() const { return m_renderSettings; }

void Editor::setResourceProvider(ResourceProvider *provider)
{
    m_resourceProvider = provider;
}

// =========================================================================
// Document accessor
// =========================================================================

const Document *Editor::document() const { return m_document.get(); }

// =========================================================================
// Mode
// =========================================================================

Editor::Mode Editor::mode() const { return m_mode; }

// =========================================================================
// Document reparsed
// =========================================================================

void Editor::onDocumentReparsed()
{
    m_document = Document::fromMarkdown(toPlainText());
    Q_EMIT headingsChanged(m_document->headings());
    Q_EMIT linksChanged(m_document->links());
    Q_EMIT tagsChanged(m_document->tags());
    Q_EMIT wordCountChanged(m_document->wordCount());
}

// =========================================================================
// Editing actions
// =========================================================================

void Editor::undo()      { if (auto *ti = focusedTextItem()) ti->textControl()->undo(); }
void Editor::redo()      { if (auto *ti = focusedTextItem()) ti->textControl()->redo(); }
void Editor::cut()       { if (auto *ti = focusedTextItem()) ti->textControl()->cut(); }
void Editor::copy()      { if (auto *ti = focusedTextItem()) ti->textControl()->copy(); }
void Editor::paste()     { if (auto *ti = focusedTextItem()) ti->textControl()->paste(); }
void Editor::selectAll() { if (auto *ti = focusedTextItem()) ti->textControl()->selectAll(); }

// =========================================================================
// Formatting actions
// =========================================================================

void Editor::wrapSelection(const QString &before, const QString &after)
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText(before + selected + after);
    } else {
        int pos = cursor.position();
        cursor.insertText(before + after);
        cursor.setPosition(pos + before.length());
        tc->setTextCursor(cursor);
    }
}

void Editor::insertAtCursor(const QString &text)
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    ti->textControl()->insertPlainText(text);
}

void Editor::toggleBold()          { wrapSelection(QStringLiteral("**"), QStringLiteral("**")); }
void Editor::toggleItalic()        { wrapSelection(QStringLiteral("*"),  QStringLiteral("*")); }
void Editor::toggleStrikethrough() { wrapSelection(QStringLiteral("~~"), QStringLiteral("~~")); }
void Editor::toggleInlineCode()    { wrapSelection(QStringLiteral("`"),  QStringLiteral("`")); }
void Editor::insertLink()          { insertAtCursor(QStringLiteral("[]()")); }
void Editor::insertWikiLink()      { insertAtCursor(QStringLiteral("[[]]")); }
void Editor::insertImage()         { insertAtCursor(QStringLiteral("![]()")); }
void Editor::insertCodeBlock()     { insertAtCursor(QStringLiteral("```\n\n```")); }
void Editor::insertBlockQuote()    { insertAtCursor(QStringLiteral("> ")); }
void Editor::insertHorizontalRule(){ insertAtCursor(QStringLiteral("\n---\n")); }
void Editor::insertCallout(const QString &type) {
    insertAtCursor(QStringLiteral("> [!%1]\n> ").arg(type));
}

void Editor::insertTable(int rows, int cols)
{
    QString table;
    // Header row
    for (int c = 0; c < cols; ++c)
        table += QStringLiteral("| Col%1 ").arg(c + 1);
    table += QStringLiteral("|\n");
    // Separator
    for (int c = 0; c < cols; ++c)
        table += QStringLiteral("|---");
    table += QStringLiteral("|\n");
    // Data rows
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols; ++c)
            table += QStringLiteral("|   ");
        table += QStringLiteral("|\n");
    }
    insertAtCursor(table);
}

void Editor::increaseHeadingLevel()
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    QString line = cursor.selectedText();
    int level = 0;
    while (level < line.size() && line.at(level) == QLatin1Char('#'))
        ++level;
    if (level < 6)
        cursor.insertText(QStringLiteral("#") + (level == 0 ? QStringLiteral(" ") : QString()) + line);
}

void Editor::decreaseHeadingLevel()
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    QString line = cursor.selectedText();
    if (!line.startsWith(QLatin1Char('#'))) return;
    cursor.insertText(line.mid(1));
}

void Editor::toggleCheckbox()
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    QString line = cursor.selectedText();
    if (line.contains(QStringLiteral("- [ ]"))) {
        cursor.insertText(line.replace(QStringLiteral("- [ ]"), QStringLiteral("- [x]")));
    } else if (line.contains(QStringLiteral("- [x]")) || line.contains(QStringLiteral("- [X]"))) {
        cursor.insertText(line.replace(QStringLiteral("- [x]"), QStringLiteral("- [ ]"))
                             .replace(QStringLiteral("- [X]"), QStringLiteral("- [ ]")));
    } else {
        cursor.insertText(QStringLiteral("- [ ] ") + line);
    }
}

// =========================================================================
// Cursor info & navigation
// =========================================================================

int Editor::cursorLine() const
{
    auto *ti = focusedTextItem();
    if (!ti) return 1;
    return ti->textControl()->textCursor().blockNumber() + 1;
}

int Editor::cursorColumn() const
{
    auto *ti = focusedTextItem();
    if (!ti) return 1;
    return ti->textControl()->textCursor().columnNumber() + 1;
}

void Editor::goToLine(int line)
{
    if (!m_coordinator) return;
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    cursor.movePosition(QTextCursor::Start);
    for (int i = 1; i < line; ++i)
        cursor.movePosition(QTextCursor::NextBlock);
    tc->setTextCursor(cursor);
    ensureFocusedCursorVisible();
}

void Editor::scrollToHeading(const HeadingInfo &heading)
{
    goToLine(heading.sourceOffset + 1);
}

// =========================================================================
// Completion trigger detection
// =========================================================================

void Editor::detectCompletionTriggers(const QString &insertedText)
{
    if (insertedText.isEmpty()) return;
    auto *ti = focusedTextItem();
    if (!ti) return;

    QTextCursor cursor = ti->textControl()->textCursor();
    int pos = cursor.positionInBlock();
    QString blockText = cursor.block().text();

    QChar ch = insertedText.at(0);
    if (ch == QLatin1Char('[') && pos >= 2 &&
        blockText.mid(pos - 2, 2) == QStringLiteral("[[")) {
        Q_EMIT wikiLinkTrigger(cursor.position());
    }
    if (ch == QLatin1Char('#') && pos > 1) {
        Q_EMIT tagTrigger(cursor.position());
    }
}

// =========================================================================
// Search
// =========================================================================

bool Editor::findText(const QString &text, QTextDocument::FindFlags flags)
{
    auto *ti = focusedTextItem();
    if (!ti) return false;
    auto *doc = ti->document();
    QTextCursor cursor = doc->find(text, ti->textControl()->textCursor(), flags);
    if (cursor.isNull()) return false;
    ti->textControl()->setTextCursor(cursor);
    return true;
}

bool Editor::replaceText(const QString &find, const QString &replace,
                         QTextDocument::FindFlags flags)
{
    auto *ti = focusedTextItem();
    if (!ti) return false;
    auto *doc = ti->document();
    QTextCursor cursor = doc->find(find, ti->textControl()->textCursor(), flags);
    if (cursor.isNull()) return false;
    cursor.insertText(replace);
    ti->textControl()->setTextCursor(cursor);
    return true;
}

int Editor::replaceAll(const QString &find, const QString &replace,
                       QTextDocument::FindFlags flags)
{
    auto *ti = focusedTextItem();
    if (!ti) return 0;
    auto *doc = ti->document();
    int count = 0;
    QTextCursor cursor(doc);
    while (!(cursor = doc->find(find, cursor, flags)).isNull()) {
        cursor.insertText(replace);
        ++count;
    }
    return count;
}

} // namespace Markoff
