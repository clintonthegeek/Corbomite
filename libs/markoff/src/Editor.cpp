// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Editor.h"
#include "markoff/SearchBar.h"
#include "SelectionScene.h"
#include "SelectionManager.h"
#include "SceneCoordinator.h"
#include "SelectableItem.h"
#include "MarkdownTextItem.h"
#include "TextControl.h"
#include "FoldingModel.h"
#include "FoldGutter.h"
#include "GutterColumn.h"

#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QScrollBar>
#include <cmath>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
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
    connect(m_coordinator, &SceneCoordinator::reparsed,
            this, &Editor::onDocumentReparsed);

    // SearchBar is a child of the Editor (not the viewport), so it
    // stays pinned to the bottom of the visible area rather than
    // scrolling with the scene contents.
    m_searchBar = new SearchBar(this);
    m_searchBar->hide();

    connect(m_searchBar, &SearchBar::searchTextChanged,
            this, &Editor::highlightAllMatches);
    connect(m_searchBar, &SearchBar::findNext, this, [this]() {
        findText(m_searchBar->searchText(), searchFlags());
        updateMatchCount();
    });
    connect(m_searchBar, &SearchBar::findPrevious, this, [this]() {
        findText(m_searchBar->searchText(),
                 searchFlags() | QTextDocument::FindBackward);
        updateMatchCount();
    });
    connect(m_searchBar, &SearchBar::replaceRequested, this, [this]() {
        replaceText(m_searchBar->searchText(), m_searchBar->replaceText(),
                    searchFlags());
        highlightAllMatches(m_searchBar->searchText());
    });
    connect(m_searchBar, &SearchBar::replaceAllRequested, this, [this]() {
        int count = replaceAll(m_searchBar->searchText(),
                               m_searchBar->replaceText(), searchFlags());
        highlightAllMatches(m_searchBar->searchText());
        Q_UNUSED(count);
    });
    connect(m_searchBar, &SearchBar::closed, this, &Editor::hideSearchBar);

    m_foldingModel = new FoldingModel(this);
    connect(this, &Editor::headingsChanged,
            m_foldingModel, &FoldingModel::reconcile);
    connect(m_foldingModel, &FoldingModel::foldStateChanged,
            this, &Editor::foldStateChanged);
    m_coordinator->setFoldingModel(m_foldingModel);

    m_foldGutter = new FoldGutter(m_foldingModel);
    m_foldGutter->setCoordinator(m_coordinator);
    m_foldGutter->setColumns({ new FoldArrowColumn(m_foldingModel) });
    m_scene->addItem(m_foldGutter);
    m_foldGutter->setZValue(1.0);  // render above text items

    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &Editor::repositionFoldGutter);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &Editor::repositionFoldGutter);

    // Cluster E Phase 2 — bridge the pixel-granular scrollbar signal to the
    // visual-line float contract. Reading scrollPositionVisualLine() at the
    // moment of emission means consumers always see a consistent value.
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) {
        Q_EMIT scrollPositionVisualLineChanged(scrollPositionVisualLine());
    });
}

Editor::~Editor() = default;

void Editor::setPlainText(const QString &text)
{
    m_sourceText = text;
    rebuildScene();
}

void Editor::clear()
{
    setPlainText({});
}

QString Editor::toPlainText() const
{
    return m_coordinator->toMarkdown();
}

void Editor::setFontSize(int pointSize)
{
    // Keep the editor's font-size knob in sync with its theme: bumping
    // the size mutates the theme's textFont and re-applies. This way
    // setTheme()/setFontSize() never disagree about which font is in use.
    m_fontSize = pointSize;
    m_theme.textFont.setPointSize(pointSize);
    if (m_coordinator) {
        m_coordinator->setFont(m_theme.textFont);
        m_coordinator->setTheme(m_theme);
    }
}

void Editor::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    qreal width = viewport()->width() - 32;
    if (width > 100)
        m_coordinator->setItemWidth(width);
    repositionSearchBar();
    repositionFoldGutter();
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

    auto *cutAction = menu.addAction(tr("Cut"), this, [this]() { cut(); });
    auto *copyAction = menu.addAction(tr("Copy"), this, [this]() { copy(); });
    auto *pasteAction = menu.addAction(tr("Paste"), this, [this]() { paste(); });

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
        // Ctrl+C / Ctrl+X / Ctrl+V — route through our copy/cut/paste
        // overrides so the clipboard gets expanded math source instead
        // of literal U+FFFC characters.
        if (e->key() == Qt::Key_C) { copy(); return; }
        if (e->key() == Qt::Key_X) { cut();  return; }
        if (e->key() == Qt::Key_V) { paste(); return; }
        if (e->key() == Qt::Key_F) { showSearchBar(); return; }
        if (e->key() == Qt::Key_H) { showReplaceBar(); return; }
    }

    bool shift = e->modifiers() & Qt::ShiftModifier;
    bool ctrl = e->modifiers() & Qt::ControlModifier;

    // F3 / Shift+F3: find next/previous (works even with bar closed)
    if (e->key() == Qt::Key_F3) {
        if (m_searchBar && !m_searchBar->searchText().isEmpty()) {
            if (shift)
                findText(m_searchBar->searchText(),
                         searchFlags() | QTextDocument::FindBackward);
            else
                findText(m_searchBar->searchText(), searchFlags());
            updateMatchCount();
        }
        return;
    }

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
        int edgePos = up ? 0 : sourceItem->documentLength();
        sourceItem->setSelection(anchorPos, edgePos);

        // Fully select all items between source and target
        const auto &items = m_coordinator->items();
        int srcIdx = items.indexOf(static_cast<SelectableItem *>(sourceItem));
        int tgtIdx = items.indexOf(static_cast<SelectableItem *>(targetItem));
        int lo = qMin(srcIdx, tgtIdx), hi = qMax(srcIdx, tgtIdx);
        for (int i = lo + 1; i < hi; ++i) {
            if (items[i]->isTextItem())
                items[i]->setSelection(0, items[i]->documentLength());
            else
                items[i]->setFullySelected(true);
        }

        // Move focus and caret to target
        targetItem->setFocus();
        QTextCursor cursor(targetItem->document());
        int entryPos = up ? targetItem->documentLength() : 0;
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
    if (m_coordinator) {
        // Apply both the per-element char formats AND the document default
        // font, otherwise body text would render at the old font size while
        // the highlighter colors come from the new theme.
        if (theme.textFont != QFont())
            m_coordinator->setFont(theme.textFont);
        m_coordinator->setTheme(theme);
    }
}

Theme Editor::theme() const { return m_theme; }

void Editor::setEditorSettings(const EditorSettings &settings)
{
    m_editorSettings = settings;
}

EditorSettings Editor::editorSettings() const { return m_editorSettings; }

void Editor::setResourceProvider(ResourceProvider *provider)
{
    m_resourceProvider = provider;
    if (m_coordinator)
        m_coordinator->setResourceProvider(provider);
}

// =========================================================================
// Read-only mode
// =========================================================================

void Editor::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    // TextBrowserInteraction allows link clicking and selection but not editing
    auto flags = readOnly ? Qt::TextBrowserInteraction : Qt::TextEditorInteraction;
    for (auto *item : m_coordinator->items()) {
        if (item->isTextItem()) {
            auto *textItem = static_cast<MarkdownTextItem *>(item->asGraphicsItem());
            textItem->textControl()->setTextInteractionFlags(flags);
        }
    }
}

bool Editor::isReadOnly() const
{
    return m_readOnly;
}

// =========================================================================
// Document accessor
// =========================================================================

const Document *Editor::document() const { return m_document.get(); }

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
void Editor::copy()
{
    // Try SelectionManager first — but only if it has a genuine cross-
    // boundary selection (m_anchorItem set). hasSelection() alone is not
    // enough because it also returns true for within-item cursor
    // selections, and serializeAsMarkdown() returns empty in that case.
    auto *mgr = m_scene->selectionManager();
    if (mgr) {
        QMimeData *data = mgr->createMimeData();
        if (data && !data->text().isEmpty()) {
            QApplication::clipboard()->setMimeData(data);
            return;
        }
        delete data;
    }
    // Fall back to the focused item's cursor selection, expanding any
    // math glyphs to raw $...$ source.
    auto *ti = focusedTextItem();
    if (!ti) return;
    const QString text = ti->selectedMarkdown();
    if (text.isEmpty()) return;
    QApplication::clipboard()->setText(text);
}

void Editor::cut()
{
    // Copy first (handles both cross-boundary and within-item).
    copy();
    // Then delete the selected content.
    auto *mgr = m_scene->selectionManager();
    if (mgr) {
        // Delete across all items that have selections.
        for (auto *item : m_coordinator->items()) {
            if (item->isTextItem()) {
                auto *ti = static_cast<MarkdownTextItem *>(item);
                QTextCursor c = ti->textControl()->textCursor();
                if (c.hasSelection())
                    c.removeSelectedText();
            }
        }
        mgr->clearSelection();
    }
}

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

        // Toggle: if the selection is already wrapped in the same delimiters,
        // strip them. e.g. **foo** + toggleBold → foo
        if (selected.size() >= before.size() + after.size()
            && selected.startsWith(before)
            && selected.endsWith(after)) {
            const QString inner = selected.mid(before.size(),
                                                selected.size() - before.size() - after.size());
            cursor.insertText(inner);
            // Reselect the now-unwrapped inner text so the user can keep
            // operating on it.
            const int newEnd = cursor.position();
            cursor.setPosition(newEnd - inner.size());
            cursor.setPosition(newEnd, QTextCursor::KeepAnchor);
            tc->setTextCursor(cursor);
            return;
        }

        // Toggle: if the chars OUTSIDE the selection (just before / just
        // after) are the delimiters, strip those instead. Lets the user
        // double-click "foo" inside `**foo**` and toggle off without
        // having to reselect the asterisks.
        const int selStart = cursor.selectionStart();
        const int selEnd = cursor.selectionEnd();
        QTextDocument *doc = tc->document();
        const QString docText = doc->toPlainText();
        if (selStart >= before.size() && selEnd + after.size() <= docText.size()
            && docText.mid(selStart - before.size(), before.size()) == before
            && docText.mid(selEnd, after.size()) == after) {
            QTextCursor outer(doc);
            outer.setPosition(selStart - before.size());
            outer.setPosition(selEnd + after.size(), QTextCursor::KeepAnchor);
            outer.insertText(selected);
            const int newEnd = outer.position();
            outer.setPosition(newEnd - selected.size());
            outer.setPosition(newEnd, QTextCursor::KeepAnchor);
            tc->setTextCursor(outer);
            return;
        }

        cursor.insertText(before + selected + after);
        return;
    }

    // No selection: insert the empty pair and place the cursor between.
    int pos = cursor.position();
    cursor.insertText(before + after);
    cursor.setPosition(pos + before.length());
    tc->setTextCursor(cursor);
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
    int startBlock = cursor.document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = cursor.document()->findBlock(cursor.selectionEnd()).blockNumber();

    cursor.beginEditBlock();
    for (int b = startBlock; b <= endBlock; ++b) {
        QTextBlock block = cursor.document()->findBlockByNumber(b);
        QTextCursor bc(block);
        bc.movePosition(QTextCursor::StartOfBlock);
        bc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = bc.selectedText();
        int level = 0;
        while (level < line.size() && line.at(level) == QLatin1Char('#'))
            ++level;
        if (level < 6)
            bc.insertText(QStringLiteral("#") + (level == 0 ? QStringLiteral(" ") : QString()) + line);
    }
    cursor.endEditBlock();
}

void Editor::decreaseHeadingLevel()
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    int startBlock = cursor.document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = cursor.document()->findBlock(cursor.selectionEnd()).blockNumber();

    cursor.beginEditBlock();
    for (int b = startBlock; b <= endBlock; ++b) {
        QTextBlock block = cursor.document()->findBlockByNumber(b);
        QTextCursor bc(block);
        bc.movePosition(QTextCursor::StartOfBlock);
        bc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = bc.selectedText();
        if (line.startsWith(QLatin1Char('#')))
            bc.insertText(line.mid(1));
    }
    cursor.endEditBlock();
}

void Editor::toggleCheckbox()
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    auto *tc = ti->textControl();
    QTextCursor cursor = tc->textCursor();
    int startBlock = cursor.document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = cursor.document()->findBlock(cursor.selectionEnd()).blockNumber();

    cursor.beginEditBlock();
    for (int b = startBlock; b <= endBlock; ++b) {
        QTextBlock block = cursor.document()->findBlockByNumber(b);
        QTextCursor bc(block);
        bc.movePosition(QTextCursor::StartOfBlock);
        bc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = bc.selectedText();
        if (line.contains(QStringLiteral("- [ ]"))) {
            bc.insertText(line.replace(QStringLiteral("- [ ]"), QStringLiteral("- [x]")));
        } else if (line.contains(QStringLiteral("- [x]")) || line.contains(QStringLiteral("- [X]"))) {
            bc.insertText(line.replace(QStringLiteral("- [x]"), QStringLiteral("- [ ]"))
                                 .replace(QStringLiteral("- [X]"), QStringLiteral("- [ ]")));
        } else {
            bc.insertText(QStringLiteral("- [ ] ") + line);
        }
    }
    cursor.endEditBlock();
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

QRect Editor::cursorScreenRect() const
{
    auto *ti = focusedTextItem();
    if (!ti) return {};
    QRectF itemRect = ti->textControl()->cursorRect();
    QRectF sceneRect = ti->mapToScene(itemRect).boundingRect();
    QPoint viewTL = mapFromScene(sceneRect.topLeft());
    QPoint viewBR = mapFromScene(sceneRect.bottomRight());
    return QRect(mapToGlobal(viewTL), mapToGlobal(viewBR));
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
    // Auto-unfold: find the heading's path and unfold any folded ancestors
    // so the target is visible before we scroll to it.
    if (m_foldingModel) {
        const auto &hs = m_foldingModel->headings();
        for (const auto &entry : hs) {
            if (entry.info.level == heading.level
                && entry.info.text == heading.text
                && entry.info.sourceOffset == heading.sourceOffset) {
                const auto unfolded = m_foldingModel->unfoldAncestors(entry.path);
                if (!unfolded.isEmpty())
                    Q_EMIT foldsAutoExpanded(unfolded);
                break;
            }
        }
    }

    // HeadingInfo::sourceOffset is a UTF-8 byte offset into the document's
    // serialized source. Convert it to a 1-based line number by counting
    // newlines up to that offset, then defer to goToLine().
    const QByteArray utf8 = toPlainText().toUtf8();
    if (heading.sourceOffset < 0 || heading.sourceOffset >= utf8.size()) {
        goToLine(1);
        return;
    }
    int line = 1;
    for (int i = 0; i < heading.sourceOffset; ++i) {
        if (utf8[i] == '\n') ++line;
    }
    goToLine(line);
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

// Walk every MarkdownTextItem in the scene in order, starting from the
// focused item (if any), so find/replace can cross item boundaries.
// Wraps around once.
static QList<MarkdownTextItem *> textItemsInSearchOrder(SceneCoordinator *coord,
                                                         MarkdownTextItem *startAfter,
                                                         bool backward)
{
    QList<MarkdownTextItem *> result;
    if (!coord) return result;
    QList<MarkdownTextItem *> all;
    for (auto *item : coord->items()) {
        if (item->isTextItem())
            all.append(static_cast<MarkdownTextItem *>(item));
    }
    if (all.isEmpty()) return result;

    int startIdx = 0;
    if (startAfter) {
        for (int i = 0; i < all.size(); ++i) {
            if (all[i] == startAfter) { startIdx = i; break; }
        }
    }
    if (backward) {
        for (int i = startIdx; i >= 0; --i) result.append(all[i]);
        for (int i = all.size() - 1; i > startIdx; --i) result.append(all[i]);
    } else {
        for (int i = startIdx; i < all.size(); ++i) result.append(all[i]);
        for (int i = 0; i < startIdx; ++i) result.append(all[i]);
    }
    return result;
}

bool Editor::findText(const QString &text, QTextDocument::FindFlags flags)
{
    if (text.isEmpty()) return false;
    const bool backward = flags & QTextDocument::FindBackward;

    auto *focused = focusedTextItem();
    auto items = textItemsInSearchOrder(m_coordinator, focused, backward);
    if (items.isEmpty()) return false;

    // Helper: search a single document, picking the right starting cursor.
    auto searchOne = [&](MarkdownTextItem *ti, bool fromCursor) -> QTextCursor {
        auto *doc = ti->document();
        QTextCursor start;
        if (fromCursor) {
            start = ti->textControl()->textCursor();
        } else {
            start = QTextCursor(doc);
            if (backward)
                start.movePosition(QTextCursor::End);
        }
        return doc->find(text, start, flags);
    };

    // Helper: given a matched item and cursor, auto-unfold ancestors if needed
    // and position the cursor on the match.
    auto commitMatch = [&](MarkdownTextItem *ti, const QTextCursor &found) {
        ti->textControl()->setTextCursor(found);
        ti->setFocus();

        // Auto-unfold: determine the enclosing heading path for the match block
        // and unfold any folded ancestors so the match is visible.
        if (m_foldingModel && m_coordinator) {
            const auto &allItems = m_coordinator->items();
            int itemIdx = -1;
            for (int i = 0; i < allItems.size(); ++i) {
                if (allItems[i] == static_cast<SelectableItem *>(ti)) {
                    itemIdx = i;
                    break;
                }
            }
            if (itemIdx >= 0) {
                const int blockNum = found.block().blockNumber();
                const QStringList path =
                    m_coordinator->enclosingHeadingPathAtBlock(itemIdx, blockNum);
                if (!path.isEmpty()) {
                    const auto unfolded = m_foldingModel->unfoldAncestors(path);
                    if (!unfolded.isEmpty())
                        Q_EMIT foldsAutoExpanded(unfolded);
                }
            }
        }
    };

    // First pass: search the focused item from the current cursor (so
    // repeated finds advance), then any other items from their edges.
    bool isFirst = true;
    for (auto *ti : items) {
        QTextCursor found = searchOne(ti, isFirst && ti == focused);
        if (!found.isNull()) {
            commitMatch(ti, found);
            return true;
        }
        isFirst = false;
    }

    // Second pass: if nothing matched and we started mid-document on the
    // focused item, wrap within that item by retrying from its edge. This
    // is what users expect from "find next" — it should wrap around even
    // for a single-item document.
    if (focused) {
        QTextCursor found = searchOne(focused, /*fromCursor=*/false);
        if (!found.isNull()) {
            commitMatch(focused, found);
            return true;
        }
    }
    return false;
}

bool Editor::replaceText(const QString &find, const QString &replace,
                         QTextDocument::FindFlags flags)
{
    auto *ti = focusedTextItem();
    if (!ti) return findText(find, flags);
    QTextCursor cursor = ti->textControl()->textCursor();
    // Replace the current selection only if it actually matches `find`.
    const bool caseSensitive = flags & QTextDocument::FindCaseSensitively;
    if (cursor.hasSelection()
        && cursor.selectedText().compare(find,
                                          caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0) {
        cursor.insertText(replace);
        ti->textControl()->setTextCursor(cursor);
    }
    // Then advance to the next match (cross-item, wrapping).
    return findText(find, flags);
}

int Editor::replaceAll(const QString &find, const QString &replace,
                       QTextDocument::FindFlags flags)
{
    if (find.isEmpty() || !m_coordinator) return 0;
    int count = 0;
    for (auto *item : m_coordinator->items()) {
        if (!item->isTextItem()) continue;
        auto *ti = static_cast<MarkdownTextItem *>(item);
        auto *doc = ti->document();
        QTextCursor cursor(doc);
        QTextCursor first = doc->find(find, cursor, flags);
        if (first.isNull()) continue;

        cursor = first;
        cursor.beginEditBlock();
        do {
            cursor.insertText(replace);
            ++count;
            cursor = doc->find(find, cursor, flags);
        } while (!cursor.isNull());
        first.endEditBlock();
    }
    return count;
}

QTextDocument::FindFlags Editor::searchFlags() const
{
    QTextDocument::FindFlags flags;
    if (m_searchBar && m_searchBar->matchCase())
        flags |= QTextDocument::FindCaseSensitively;
    return flags;
}

void Editor::showSearchBar()
{
    if (auto *ti = focusedTextItem()) {
        QString sel = ti->textControl()->textCursor().selectedText();
        if (!sel.isEmpty() && !sel.contains(QChar::ParagraphSeparator))
            m_searchBar->setSearchText(sel);
    }
    m_searchBar->showFind();
    repositionSearchBar();
}

void Editor::showReplaceBar()
{
    if (auto *ti = focusedTextItem()) {
        QString sel = ti->textControl()->textCursor().selectedText();
        if (!sel.isEmpty() && !sel.contains(QChar::ParagraphSeparator))
            m_searchBar->setSearchText(sel);
    }
    m_searchBar->showReplace();
    repositionSearchBar();
}

void Editor::hideSearchBar()
{
    m_searchBar->hide();
    setViewportMargins(0, 0, 0, 0);
    clearSearchHighlights();
    setFocus();
}

void Editor::repositionSearchBar()
{
    if (!m_searchBar->isVisible()) {
        setViewportMargins(0, 0, 0, 0);
        return;
    }
    int barHeight = m_searchBar->sizeHint().height();
    // Reserve space at the bottom of the viewport so scene content
    // cannot scroll under the bar.
    setViewportMargins(0, 0, 0, barHeight);
    // Position the bar in the reserved strip, spanning the viewport's
    // horizontal extent (so it doesn't cover the vertical scrollbar).
    QPoint vpTopLeft = viewport()->mapTo(this, QPoint(0, 0));
    int vw = viewport()->width();
    int vh = viewport()->height();
    m_searchBar->setGeometry(vpTopLeft.x(), vpTopLeft.y() + vh,
                             vw, barHeight);
    m_searchBar->raise();
}

void Editor::highlightAllMatches(const QString &text)
{
    clearSearchHighlights();
    m_lastSearchText = text;
    m_totalMatchCount = 0;
    m_currentMatchIndex = -1;

    if (text.isEmpty() || !m_coordinator) {
        m_searchBar->setMatchCount(0, 0);
        return;
    }

    QTextDocument::FindFlags flags = searchFlags();

    // Highlight formats
    QTextCharFormat matchFmt;
    matchFmt.setBackground(QColor(255, 255, 0, 120)); // yellow

    QTextCharFormat currentFmt;
    currentFmt.setBackground(QColor(255, 150, 50, 180)); // orange

    // Find the focused item and cursor position for current-match tracking
    auto *focusedItem = focusedTextItem();
    int focusedCursorPos = -1;
    if (focusedItem)
        focusedCursorPos = focusedItem->textControl()->textCursor().position();

    int globalIndex = 0;
    int closestIndex = -1;
    int closestDistance = std::numeric_limits<int>::max();
    MarkdownTextItem *closestItem = nullptr;
    QTextCursor closestCursor;

    for (auto *item : m_coordinator->items()) {
        if (!item->isTextItem()) continue;
        auto *ti = static_cast<MarkdownTextItem *>(item);
        auto *doc = ti->document();
        QList<QTextEdit::ExtraSelection> selections;

        QTextCursor search(doc);
        while (!(search = doc->find(text, search, flags)).isNull()) {
            if (m_totalMatchCount >= 65536) break;

            QTextEdit::ExtraSelection sel;
            sel.cursor = search;
            sel.format = matchFmt;
            selections.append(sel);

            // Track closest match to cursor for current-match index
            if (ti == focusedItem && focusedCursorPos >= 0) {
                int dist = search.selectionStart() - focusedCursorPos;
                if (dist >= 0 && dist < closestDistance) {
                    closestDistance = dist;
                    closestIndex = globalIndex;
                    closestItem = ti;
                    closestCursor = search;
                }
            }

            ++globalIndex;
            ++m_totalMatchCount;
        }

        ti->textControl()->setExtraSelections(selections);
    }

    // If no match found at/after cursor, use the first match
    if (closestIndex < 0 && m_totalMatchCount > 0)
        closestIndex = 0;

    m_currentMatchIndex = closestIndex;

    // Highlight current match in orange
    if (closestItem && closestCursor.hasSelection()) {
        auto rawSelections = closestItem->textControl()->extraSelections();
        for (int i = 0; i < rawSelections.size(); ++i) {
            if (rawSelections[i].cursor.selectionStart() == closestCursor.selectionStart()
                && rawSelections[i].cursor.selectionEnd() == closestCursor.selectionEnd()) {
                rawSelections[i].format = currentFmt;
            }
        }
        closestItem->textControl()->setExtraSelections(rawSelections);
    }

    m_searchBar->setMatchCount(
        m_totalMatchCount > 0 ? m_currentMatchIndex + 1 : 0,
        m_totalMatchCount);
}

void Editor::clearSearchHighlights()
{
    if (!m_coordinator) return;
    const QList<QTextEdit::ExtraSelection> empty;
    for (auto *item : m_coordinator->items()) {
        if (!item->isTextItem()) continue;
        auto *ti = static_cast<MarkdownTextItem *>(item);
        ti->textControl()->setExtraSelections(empty);
    }
    m_totalMatchCount = 0;
    m_currentMatchIndex = -1;
}

void Editor::updateMatchCount()
{
    if (m_lastSearchText.isEmpty() || m_totalMatchCount == 0) return;

    // Recompute current index based on cursor position
    auto *focusedItem = focusedTextItem();
    if (!focusedItem) return;
    int cursorPos = focusedItem->textControl()->textCursor().selectionStart();

    int index = 0;
    QTextDocument::FindFlags flags = searchFlags();
    for (auto *item : m_coordinator->items()) {
        if (!item->isTextItem()) continue;
        auto *ti = static_cast<MarkdownTextItem *>(item);
        auto *doc = ti->document();
        QTextCursor search(doc);
        while (!(search = doc->find(m_lastSearchText, search, flags)).isNull()) {
            if (ti == focusedItem && search.selectionStart() == cursorPos) {
                m_currentMatchIndex = index;
                m_searchBar->setMatchCount(index + 1, m_totalMatchCount);

                // Update orange highlight
                highlightAllMatches(m_lastSearchText);
                return;
            }
            ++index;
        }
    }
}

// =========================================================================
// Folding public API
// =========================================================================

QList<QStringList> Editor::headingPaths() const
{
    return m_foldingModel->allPaths();
}

bool Editor::isFolded(const QStringList &path) const
{
    return m_foldingModel->isFolded(path);
}

QList<QStringList> Editor::foldedPaths() const
{
    return m_foldingModel->foldedPaths();
}

void Editor::fold(const QStringList &path)
{
    m_foldingModel->fold(path);
}

void Editor::unfold(const QStringList &path)
{
    m_foldingModel->unfold(path);
}

void Editor::toggleFold(const QStringList &path)
{
    m_foldingModel->toggle(path);
}

void Editor::toggleFoldAtCursor()
{
    // HeadingInfo::sourceOffset is a UTF-8 byte offset (same unit as used in
    // scrollToHeading). Convert cursor line to a byte offset range check by
    // finding the byte offset of the start of cursorLine(), then pick the
    // last heading whose sourceOffset is <= that byte offset.
    const int line = cursorLine(); // 1-based
    const QByteArray utf8 = toPlainText().toUtf8();

    // Compute the byte offset of the start of `line` (1-based).
    int lineStart = 0;
    int currentLine = 1;
    for (int i = 0; i < utf8.size() && currentLine < line; ++i) {
        if (utf8[i] == '\n') {
            ++currentLine;
            if (currentLine == line) {
                lineStart = i + 1;
                break;
            }
        }
    }

    const auto &hs = m_foldingModel->headings();
    const FoldingModel::HeadingEntry *best = nullptr;
    for (const auto &h : hs) {
        if (h.info.sourceOffset <= lineStart)
            best = &h;
        else
            break;
    }
    if (best)
        m_foldingModel->toggle(best->path);
}

void Editor::foldAll()          { m_foldingModel->foldAll(); }
void Editor::unfoldAll()        { m_foldingModel->unfoldAll(); }
void Editor::foldAllAtLevel(int level)   { m_foldingModel->foldAllAtLevel(level); }
void Editor::unfoldAllAtLevel(int level) { m_foldingModel->unfoldAllAtLevel(level); }
void Editor::foldLevel(int n)   { m_foldingModel->foldLevel(n); }
void Editor::unfoldLevel(int n) { m_foldingModel->unfoldLevel(n); }

QJsonObject Editor::serializeFoldState() const
{
    return m_foldingModel->serialize();
}

void Editor::restoreFoldState(const QJsonObject &state)
{
    m_foldingModel->restore(state);
}

void Editor::repositionFoldGutter()
{
    if (m_foldGutter)
        m_foldGutter->setPos(mapToScene(viewport()->rect().topLeft()));
}

void Editor::setGutterVisible(bool visible)
{
    m_gutterVisible = visible;
    if (m_foldGutter)
        m_foldGutter->setVisible(visible);
}

bool Editor::isGutterVisible() const
{
    return m_gutterVisible;
}

// ============================================================================
// Visual-line float scroll (Cluster E Phase 2)
// ============================================================================
//
// Markoff is a QGraphicsView-backed editor where each block (paragraph, table,
// image, code block, ...) is one QGraphicsItem in a vertically-stacked scene.
// A "visual line" here is one wrapped display line's worth of vertical space.
// For a block item we approximate its visual-line span as
// `ceil(boundingRect().height() / lineHeight)`, where `lineHeight` is the
// theme's base textFont line spacing. This is uniform across all block types
// (no per-class virtual method) because every block already publishes its
// height through `boundingRect()` — approximation is intentional: the
// ±0.5-visual-line contract tolerates the difference between a 1.5-line-high
// heading and a 2-line slot, and avoiding a virtual-method plumbing pass
// keeps the Phase 2 change surgical per the plan.
//
// The scene-Y ⇄ visual-line mapping is computed on-call by walking the
// coordinator's items list in display order. Input sizes are modest
// (hundreds of blocks), so a linear walk is cheaper than maintaining a
// persistent cumsum across every reparse.

namespace {

qreal editorLineHeight(const Markoff::Theme &theme)
{
    QFontMetricsF fm(theme.textFont);
    const qreal h = fm.lineSpacing();
    return h > 1.0 ? h : 16.0;
}

} // namespace

float Editor::scrollPositionVisualLine() const
{
    if (!m_coordinator)
        return 0.0f;
    const auto &items = m_coordinator->items();
    if (items.isEmpty())
        return 0.0f;

    const qreal lineH = editorLineHeight(m_theme);
    const qreal y = verticalScrollBar()->value();

    qreal linesSoFar = 0.0;
    for (auto *item : items) {
        QGraphicsItem *gi = item->asGraphicsItem();
        if (!gi || !gi->isVisible())
            continue;
        const QRectF r = gi->sceneBoundingRect();
        const qreal blockLines = std::max<qreal>(1.0, std::ceil(r.height() / lineH));
        if (y < r.bottom()) {
            // `y` is inside or above this block; compute fractional position.
            const qreal offset = std::max<qreal>(0.0, y - r.top());
            const qreal frac = std::min<qreal>(blockLines, offset / lineH);
            return static_cast<float>(linesSoFar + frac);
        }
        linesSoFar += blockLines;
    }
    return static_cast<float>(linesSoFar);
}

void Editor::setScrollPositionVisualLine(float visualLine)
{
    if (!m_coordinator)
        return;
    const auto &items = m_coordinator->items();
    if (items.isEmpty())
        return;

    const qreal lineH = editorLineHeight(m_theme);
    const qreal target = std::max<qreal>(0.0, visualLine);

    qreal linesSoFar = 0.0;
    qreal pixelY = 0.0;
    bool placed = false;
    for (auto *item : items) {
        QGraphicsItem *gi = item->asGraphicsItem();
        if (!gi || !gi->isVisible())
            continue;
        const QRectF r = gi->sceneBoundingRect();
        const qreal blockLines = std::max<qreal>(1.0, std::ceil(r.height() / lineH));
        if (target <= linesSoFar + blockLines) {
            const qreal insideLines = target - linesSoFar;
            pixelY = r.top() + insideLines * lineH;
            placed = true;
            break;
        }
        linesSoFar += blockLines;
        pixelY = r.bottom();
    }
    if (!placed) {
        // Past the end: clamp to the last block's bottom.
        pixelY = std::max<qreal>(0.0, pixelY);
    }

    QScrollBar *vbar = verticalScrollBar();
    const int clamped = std::clamp<int>(static_cast<int>(std::round(pixelY)),
                                        vbar->minimum(), vbar->maximum());
    vbar->setValue(clamped);
}

} // namespace Markoff
