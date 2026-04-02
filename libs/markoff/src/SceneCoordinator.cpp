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
        moveFocusTo(item, edge);
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
    qreal y = 0;
    for (auto *item : m_items) {
        QGraphicsItem *gi = item->asGraphicsItem();
        gi->setPos(0, y);
        y += gi->boundingRect().height() + m_spacing;
    }
    m_scene->setSceneRect(0, 0, m_itemWidth, y);
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
