// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneCoordinator.h"
#include "SelectionScene.h"
#include "SelectableItem.h"
#include "MarkdownTextItem.h"
#include "TextControl.h"
#include "StubBlockItem.h"
#include "TableBlockItem.h"
#include "MarkdownSplitter.h"
#include "MarkdownHighlighter.h"
#include "TreeSitterParser.h"

#include <QTimer>
#include <QTextDocument>
#include <QGuiApplication>

namespace Markoff {

SceneCoordinator::SceneCoordinator(SelectionScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_parser(new TreeSitterParser)
{
    m_reparseTimer = new QTimer(this);
    m_reparseTimer->setSingleShot(true);
    m_reparseTimer->setInterval(150);
    connect(m_reparseTimer, &QTimer::timeout, this, &SceneCoordinator::reparse);
}

SceneCoordinator::~SceneCoordinator()
{
    delete m_parser;
}

MarkdownTextItem *SceneCoordinator::createTextItem(const QString &text,
                                                     MarkdownHighlighter::Mode hlMode)
{
    auto *item = new MarkdownTextItem;
    item->setTextWidth(m_itemWidth);
    if (m_font.pointSize() > 0)
        item->document()->setDefaultFont(m_font);

    auto *highlighter = new MarkdownHighlighter(item->document());
    highlighter->setMode(hlMode);

    // Set span map and decorated ranges BEFORE setPlainText. When
    // setPlainText triggers Qt's automatic highlightBlock calls,
    // both the span map and decorated ranges must already be available
    // for correct syntax coloring on the first render.
    if (m_parser->parse(text))
        highlighter->setSpanMap(m_parser->buildSpanMap());

    // Pre-detect decorated ranges from raw text so the highlighter
    // has them during the initial highlight pass.
    item->setPlainText(text);
    highlighter->setDecoratedRanges(item->decoratedRanges());
    highlighter->rehighlight();

    // Connect incremental span offset adjustment. Fires on every
    // document change BEFORE Qt's auto-rehighlight, keeping the
    // span map approximately correct between full reparses.
    connect(item->document(), &QTextDocument::contentsChange,
            highlighter, &MarkdownHighlighter::adjustSpanOffsets);

    m_scene->addItem(item);
    m_items.append(item);

    connect(item, &MarkdownTextItem::textChanged,
            this, &SceneCoordinator::onItemTextChanged);
    connect(item, &MarkdownTextItem::cursorAtBoundary,
            this, [this, item](Qt::Edge edge) {
        handleBoundary(item, edge);
    });

    return item;
}

void SceneCoordinator::loadSource(const QString &markdown)
{
    clearItems();
    createTextItem(markdown, MarkdownHighlighter::Mode::Source);
    repositionItems();
    m_scene->setSelectableItems(m_items);
}

void SceneCoordinator::loadMarkdown(const QString &markdown)
{
    clearItems();

    auto segments = MarkdownSplitter::split(markdown, *m_parser);

    for (const auto &seg : segments) {
        if (seg.type == MarkdownSegment::Text) {
            createTextItem(seg.text, MarkdownHighlighter::Mode::LivePreview);
        } else {
            auto *item = new TableBlockItem(seg.text, m_itemWidth);
            m_scene->addItem(item);
            m_items.append(item);
        }
    }

    repositionItems();
    m_scene->setSelectableItems(m_items);
}

QString SceneCoordinator::toMarkdown() const
{
    QString result;
    for (int i = 0; i < m_items.size(); ++i) {
        if (i > 0)
            result += QLatin1Char('\n');
        result += m_items[i]->toMarkdown();
        if (i < m_items.size() - 1)
            result += QLatin1Char('\n');
    }
    return result;
}

void SceneCoordinator::setItemWidth(qreal width)
{
    if (qFuzzyCompare(m_itemWidth, width))
        return;
    m_itemWidth = width;

    for (auto *item : m_items) {
        if (item->isTextItem()) {
            auto *textItem = static_cast<MarkdownTextItem *>(item);
            textItem->setTextWidth(width);
        }
        // StubBlockItems have fixed width — they'll be replaced by real items later
    }
    repositionItems();
}

void SceneCoordinator::setTheme(const Theme &theme)
{
    for (auto *item : m_items) {
        if (item->isTextItem()) {
            auto *textItem = static_cast<MarkdownTextItem *>(item);
            auto *highlighter = qobject_cast<MarkdownHighlighter *>(
                textItem->document()->findChild<QSyntaxHighlighter *>());
            if (highlighter)
                highlighter->setTheme(theme);
        }
    }
}

void SceneCoordinator::setFont(const QFont &font)
{
    m_font = font;
    for (auto *item : m_items) {
        if (item->isTextItem()) {
            auto *textItem = static_cast<MarkdownTextItem *>(item);
            textItem->document()->setDefaultFont(font);
        } else if (auto *table = dynamic_cast<TableBlockItem *>(item->asGraphicsItem())) {
            table->setFont(font);
        }
    }
    repositionItems();
}

bool SceneCoordinator::moveFocusTo(MarkdownTextItem *from, Qt::Edge edge)
{
    int idx = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i] == from) { idx = i; break; }
    }
    if (idx < 0) return false;

    // Find the next text item in the given direction
    int delta = (edge == Qt::TopEdge) ? -1 : 1;
    for (int i = idx + delta; i >= 0 && i < m_items.size(); i += delta) {
        if (m_items[i]->isTextItem()) {
            auto *target = static_cast<MarkdownTextItem *>(m_items[i]);
            target->setFocus();
            // Place cursor at appropriate end
            QTextCursor cursor(target->document());
            if (edge == Qt::TopEdge)
                cursor.movePosition(QTextCursor::End);
            else
                cursor.movePosition(QTextCursor::Start);
            target->textControl()->setTextCursor(cursor);
            return true;
        }
    }
    return false;
}

void SceneCoordinator::handleBoundary(MarkdownTextItem *from, Qt::Edge edge)
{
    bool shiftHeld = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
    auto *mgr = m_scene->selectionManager();

    if (!shiftHeld) {
        if (mgr->mode() == SelectionMode::CrossBoundary) {
            mgr->clearSelection();
            m_keyboardCurrentIdx = -1;
            m_keyboardAnchorIdx = -1;
        }
        moveFocusTo(from, edge);
        return;
    }

    int dir = (edge == Qt::BottomEdge) ? 1 : -1;
    int fromIdx = m_items.indexOf(static_cast<SelectableItem *>(from));
    if (fromIdx < 0) return;

    // First time entering cross-boundary: record anchor
    if (mgr->mode() != SelectionMode::CrossBoundary) {
        QTextCursor cursor = from->textControl()->textCursor();
        m_keyboardAnchorPos = cursor.anchor();
        m_keyboardAnchorIdx = fromIdx;
        m_keyboardCurrentIdx = fromIdx;

        // Finalize anchor item's selection to its edge
        int edgePos = (edge == Qt::BottomEdge)
            ? from->allMarkdown().length() : 0;
        from->setSelection(m_keyboardAnchorPos, edgePos);
        mgr->beginOrExtendKeyboardSelection(from, m_keyboardAnchorPos, from, edgePos);
    }

    // Determine if extending (away from anchor) or contracting (toward anchor)
    bool extending = (dir > 0 && m_keyboardCurrentIdx >= m_keyboardAnchorIdx)
                  || (dir < 0 && m_keyboardCurrentIdx <= m_keyboardAnchorIdx);

    int nextIdx = m_keyboardCurrentIdx + dir;
    if (nextIdx < 0 || nextIdx >= m_items.size())
        return;

    if (extending) {
        // Advancing away from anchor — select the next item
        auto *next = m_items[nextIdx];
        m_keyboardCurrentIdx = nextIdx;

        if (next->isTextItem()) {
            // Transfer focus, place caret at entry edge
            auto *textItem = static_cast<MarkdownTextItem *>(next);
            textItem->setFocus();
            QTextCursor cursor(textItem->document());
            if (edge == Qt::BottomEdge)
                cursor.movePosition(QTextCursor::Start);
            else
                cursor.movePosition(QTextCursor::End);
            textItem->textControl()->setTextCursor(cursor);
        } else {
            // Block item — fully select it, focus stays on current text item
            next->setFullySelected(true);
        }

        mgr->beginOrExtendKeyboardSelection(
            mgr->anchorItem(), -1, m_items[nextIdx],
            next->isTextItem() ? 0 : -1);

    } else {
        // Contracting back toward anchor — deselect the current item
        auto *departing = m_items[m_keyboardCurrentIdx];
        if (departing->isTextItem())
            departing->clearSelection();
        else
            departing->setFullySelected(false);

        m_keyboardCurrentIdx = nextIdx;
        auto *arriving = m_items[m_keyboardCurrentIdx];

        if (m_keyboardCurrentIdx == m_keyboardAnchorIdx) {
            // Returned to anchor item — restore within-item selection
            auto *anchorText = static_cast<MarkdownTextItem *>(arriving);
            anchorText->setFocus();
            QTextCursor cursor(anchorText->document());
            cursor.setPosition(m_keyboardAnchorPos);
            // Position at the edge we arrived from
            int edgePos = (edge == Qt::BottomEdge)
                ? 0 : anchorText->allMarkdown().length();
            cursor.setPosition(edgePos, QTextCursor::KeepAnchor);
            anchorText->textControl()->setTextCursor(cursor);

            // Exit cross-boundary mode — back to within-item
            mgr->clearSelection();
            // Re-apply the within-item selection we just set
            anchorText->setSelection(m_keyboardAnchorPos, edgePos);
            m_keyboardCurrentIdx = -1;
            m_keyboardAnchorIdx = -1;

        } else if (arriving->isTextItem()) {
            // Arrived at a non-anchor text item — transfer focus
            auto *textItem = static_cast<MarkdownTextItem *>(arriving);
            textItem->setFocus();
            // Place caret at the edge we arrived from, with full selection
            QTextCursor cursor(textItem->document());
            if (edge == Qt::BottomEdge) {
                // Contracting upward, arriving from below
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            } else {
                // Contracting downward, arriving from above
                cursor.movePosition(QTextCursor::End);
                cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
            }
            textItem->textControl()->setTextCursor(cursor);
            mgr->beginOrExtendKeyboardSelection(
                mgr->anchorItem(), -1, arriving, cursor.position());

        } else {
            // Arrived at a block — it stays selected (we just deselected the one beyond it)
            // Focus stays on the text item that still has focus
        }
    }
}

void SceneCoordinator::clearItems()
{
    for (auto *item : m_items) {
        m_scene->removeItem(item->asGraphicsItem());
        delete item->asGraphicsItem();
    }
    m_items.clear();
}

void SceneCoordinator::repositionItems()
{
    qreal y = m_topMargin;
    for (auto *item : m_items) {
        QGraphicsItem *gi = item->asGraphicsItem();
        gi->setPos(m_leftMargin, y);
        y += gi->boundingRect().height() + m_spacing;
    }
    m_scene->setSceneRect(0, 0, m_leftMargin + m_itemWidth + m_leftMargin, y + m_topMargin);
}

void SceneCoordinator::onItemTextChanged()
{
    if (m_inReparse)
        return;
    m_reparseTimer->start(); // restart 150ms countdown
    emit textChanged();
}

void SceneCoordinator::reparse()
{
    m_inReparse = true;

    // Serialize current state
    QString markdown = toMarkdown();

    // Check if block boundaries changed
    auto newSegments = MarkdownSplitter::split(markdown, *m_parser);

    // Compare segment count and types to current items
    bool structureChanged = false;
    if (newSegments.size() != m_items.size()) {
        structureChanged = true;
    } else {
        for (int i = 0; i < newSegments.size(); ++i) {
            bool wasText = m_items[i]->isTextItem();
            bool isText = (newSegments[i].type == MarkdownSegment::Text);
            if (wasText != isText) {
                structureChanged = true;
                break;
            }
        }
    }

    if (structureChanged) {
        clearItems();
        for (const auto &seg : newSegments) {
            if (seg.type == MarkdownSegment::Text) {
                createTextItem(seg.text, MarkdownHighlighter::Mode::LivePreview);
            } else {
                auto *item = new TableBlockItem(seg.text, m_itemWidth);
                m_scene->addItem(item);
                m_items.append(item);
            }
        }
        repositionItems();
        m_scene->setSelectableItems(m_items);
    } else {
        // Structure unchanged — update span maps using the shared parser.
        // Don't call rehighlight() — the adjustSpanOffsets connection keeps
        // spans approximately correct, and setSpanMap + targeted block
        // rehighlight on the next cursor-driven repaint is sufficient.
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i]->isTextItem()) {
                auto *textItem = static_cast<MarkdownTextItem *>(m_items[i]);
                if (m_parser->parse(textItem->allMarkdown())) {
                    auto *highlighter = qobject_cast<MarkdownHighlighter *>(
                        textItem->document()->findChild<QSyntaxHighlighter *>());
                    if (highlighter)
                        highlighter->setSpanMap(m_parser->buildSpanMap());
                }
            }
        }
        repositionItems();
    }

    // Clear inReparse on next event loop so deferred signals are suppressed
    QTimer::singleShot(0, this, [this]() {
        m_inReparse = false;
    });
    emit reparsed();
}

} // namespace Markoff
