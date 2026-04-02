// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneCoordinator.h"
#include "SelectionScene.h"
#include "SelectableItem.h"
#include "MarkdownTextItem.h"
#include "TextControl.h"
#include "StubBlockItem.h"
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
    item->setPlainText(text);
    if (m_font.pointSize() > 0)
        item->document()->setDefaultFont(m_font);

    auto *highlighter = new MarkdownHighlighter(item->document());
    highlighter->setMode(hlMode);

    // Parse this item's text for its own span map
    TreeSitterParser itemParser;
    if (itemParser.parse(text)) {
        highlighter->setSpanMap(itemParser.buildSpanMap());
        highlighter->rehighlight(); // force initial highlight with span map
    }

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
            auto *item = new StubBlockItem(seg.text, m_itemWidth, 80);
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

void SceneCoordinator::setFont(const QFont &font)
{
    m_font = font;
    for (auto *item : m_items) {
        if (item->isTextItem()) {
            auto *textItem = static_cast<MarkdownTextItem *>(item);
            textItem->document()->setDefaultFont(font);
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
        if (mgr->mode() == SelectionMode::CrossBoundary)
            mgr->clearSelection();
        moveFocusTo(from, edge);
        return;
    }

    // Shift+Arrow at boundary — caret must move to the next text item.
    int dir = (edge == Qt::BottomEdge) ? 1 : -1;
    int fromIdx = m_items.indexOf(static_cast<SelectableItem *>(from));
    if (fromIdx < 0) return;

    // First time entering cross-boundary: record anchor and finalize
    // the departing item's selection from anchor to its edge.
    if (mgr->mode() != SelectionMode::CrossBoundary) {
        QTextCursor cursor = from->textControl()->textCursor();
        int anchorPos = cursor.anchor();
        int edgePos = (edge == Qt::BottomEdge)
            ? from->allMarkdown().length() : 0;
        from->setSelection(anchorPos, edgePos);

        mgr->beginOrExtendKeyboardSelection(from, anchorPos, from, edgePos);
    } else {
        // Already in cross-boundary. The item we're leaving is fully
        // traversed — select it fully (unless it's the anchor).
        if (from != mgr->anchorItem()) {
            from->setSelection(0, from->allMarkdown().length());
        }
    }

    // Walk from current position, marking blocks as fully selected,
    // until we find the next text item.
    MarkdownTextItem *nextText = nullptr;
    for (int i = fromIdx + dir; i >= 0 && i < m_items.size(); i += dir) {
        auto *item = m_items[i];
        if (item->isTextItem()) {
            nextText = static_cast<MarkdownTextItem *>(item);
            break;
        } else {
            // Block item — fully select it
            item->setFullySelected(true);
        }
    }
    if (!nextText) return;

    // Move focus and caret to the target text item.
    nextText->setFocus();
    QTextCursor cursor(nextText->document());
    if (edge == Qt::BottomEdge)
        cursor.movePosition(QTextCursor::Start);
    else
        cursor.movePosition(QTextCursor::End);
    nextText->textControl()->setTextCursor(cursor);

    // Update SelectionManager endpoint (but don't call applySelection —
    // we've already set all the visuals directly above, and the target
    // item's selection is managed by its TextControl as the user keeps
    // pressing Shift+Arrow).
    mgr->beginOrExtendKeyboardSelection(
        mgr->anchorItem(), -1, // keep existing anchor
        nextText, cursor.position());
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
                auto *item = new StubBlockItem(seg.text, m_itemWidth, 80);
                m_scene->addItem(item);
                m_items.append(item);
            }
        }
        repositionItems();
        m_scene->setSelectableItems(m_items);
    } else {
        // Structure unchanged — just update span maps for each text item
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i]->isTextItem()) {
                auto *textItem = static_cast<MarkdownTextItem *>(m_items[i]);
                TreeSitterParser itemParser;
                if (itemParser.parse(textItem->allMarkdown())) {
                    auto *highlighter = qobject_cast<MarkdownHighlighter *>(
                        textItem->document()->findChild<QSyntaxHighlighter *>());
                    if (highlighter) {
                        highlighter->setSpanMap(itemParser.buildSpanMap());
                        highlighter->rehighlight();
                    }
                }
            }
        }
        repositionItems();
    }

    // Clear inReparse on next event loop so deferred signals are suppressed
    QTimer::singleShot(0, this, [this]() {
        m_inReparse = false;
    });
}

} // namespace Markoff
