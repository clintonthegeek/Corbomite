// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneCoordinator.h"
#include "SelectionScene.h"
#include "SelectableItem.h"
#include "MarkdownTextItem.h"
#include "TextControl.h"
#include "TableBlockItem.h"
#include "ImageBlockItem.h"
#include "FoldingModel.h"
#include <markoff-parser/MarkdownSplitter.h>
#include "MarkdownHighlighter.h"
#include <markoff-parser/TreeSitterParser.h>

#include <QTimer>
#include <QTextDocument>
#include <QGuiApplication>

namespace Markoff {

namespace {
/// Count newlines in `utf8` up to and including byte offset `byteOffset`.
/// The returned line index is 0-based: byteOffset 0 → line 0, byteOffset
/// at the first byte after the first '\n' → line 1, etc.
int sourceLineAt(const QByteArray &utf8, int byteOffset)
{
    const int end = qBound(0, byteOffset, int(utf8.size()));
    int lines = 0;
    for (int i = 0; i < end; ++i) {
        if (utf8[i] == '\n') ++lines;
    }
    return lines;
}
} // namespace


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

MarkdownTextItem *SceneCoordinator::createTextItem(const QString &text)
{
    auto *item = new MarkdownTextItem;
    item->setTextWidth(m_itemWidth);
    if (m_font.pointSize() > 0)
        item->document()->setDefaultFont(m_font);

    auto *highlighter = new MarkdownHighlighter(item->document());

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

    // Replace inline-math source with rendered glyphs.
    // The spans are now set, so the substitution can find them.
    item->refreshInlineSubstitutions();

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

void SceneCoordinator::loadMarkdown(const QString &markdown)
{
    clearItems();

    auto segments = MarkdownSplitter::split(markdown, *m_parser);

    for (const auto &seg : segments) {
        if (seg.type == MarkdownSegment::Text) {
            createTextItem(seg.text);
        } else if (seg.type == MarkdownSegment::Image) {
            auto *item = new ImageBlockItem(seg.text, m_itemWidth, m_resourceProvider);
            m_scene->addItem(item);
            m_items.append(item);
        } else {
            auto *item = new TableBlockItem(seg.text, m_itemWidth);
            m_scene->addItem(item);
            m_items.append(item);
        }
    }

    repositionItems();
    m_scene->setSelectableItems(m_items);
    m_headingMapDirty = true;
    emit reparsed();
}

QString SceneCoordinator::toMarkdown() const
{
    QString result;
    for (int i = 0; i < m_items.size(); ++i) {
        if (i > 0) {
            // Block items (images, tables) need a blank line separator
            // to preserve the original markdown structure.
            bool prevIsBlock = !m_items[i - 1]->isTextItem();
            bool currIsBlock = !m_items[i]->isTextItem();
            result += (prevIsBlock || currIsBlock)
                ? QStringLiteral("\n\n") : QStringLiteral("\n");
        }
        result += m_items[i]->toMarkdown();
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

void SceneCoordinator::setResourceProvider(ResourceProvider *provider)
{
    m_resourceProvider = provider;
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
            ? from->documentLength() : 0;
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
                ? 0 : anchorText->documentLength();
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
                createTextItem(seg.text);
            } else if (seg.type == MarkdownSegment::Image) {
                auto *item = new ImageBlockItem(seg.text, m_itemWidth, m_resourceProvider);
                m_scene->addItem(item);
                m_items.append(item);
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
        //
        // Math substitution: each text item may currently contain U+FFFC
        // glyphs in place of $...$ regions. We strip them before applying
        // the new span map (so document offsets line up with span offsets),
        // then re-substitute afterwards.
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items[i]->isTextItem()) {
                auto *textItem = static_cast<MarkdownTextItem *>(m_items[i]);
                const QString src = textItem->allMarkdown();

                // Block document signals for the entire update cycle.
                // This prevents intermediate rehighlights between strip
                // (source form) and apply (substituted form) from leaving
                // formatting at stale character positions.
                QTextDocument *doc = textItem->document();
                const bool blocked = doc->blockSignals(true);

                textItem->stripInlineSubstitutions();
                if (m_parser->parse(src)) {
                    auto *highlighter = qobject_cast<MarkdownHighlighter *>(
                        doc->findChild<QSyntaxHighlighter *>());
                    if (highlighter)
                        highlighter->setSpanMap(m_parser->buildSpanMap());
                }
                textItem->refreshBlockFormatting();
                textItem->refreshInlineSubstitutions();

                doc->blockSignals(blocked);
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

void SceneCoordinator::setFoldingModel(FoldingModel *model)
{
    if (m_foldingModel)
        disconnect(m_foldingModel, nullptr, this, nullptr);
    m_foldingModel = model;
    m_headingMapDirty = true;
    if (m_foldingModel) {
        connect(m_foldingModel, &FoldingModel::foldStateChanged,
                this, [this]() {
                    m_headingMapDirty = true;
                    applyFoldVisibility();
                });
    }
}

void SceneCoordinator::ensureHeadingMap() const
{
    if (!m_headingMapDirty) return;
    m_blockToHeadingIdx.clear();
    m_headingMapDirty = false;
    if (!m_foldingModel) return;
    const auto &hs = m_foldingModel->headings();
    if (hs.isEmpty()) return;

    // Heading sourceOffsets are byte offsets into toMarkdown()'s output
    // (that's the source tree-sitter parses). Convert each to a 0-based
    // source-line for line-based comparison against item blocks.
    const QByteArray utf8 = toMarkdown().toUtf8();
    QHash<int, int> lineToHeadingIdx;
    lineToHeadingIdx.reserve(hs.size());
    for (int i = 0; i < hs.size(); ++i)
        lineToHeadingIdx.insert(sourceLineAt(utf8, hs[i].info.sourceOffset), i);

    // Walk items in order, tracking the running source-line offset.
    // Mirror toMarkdown()'s separator rules so line counts align with
    // what tree-sitter saw.
    int srcLine = 0;
    for (int itemIdx = 0; itemIdx < m_items.size(); ++itemIdx) {
        if (itemIdx > 0) {
            const bool prevBlock = !m_items[itemIdx - 1]->isTextItem();
            const bool currBlock = !m_items[itemIdx]->isTextItem();
            srcLine += (prevBlock || currBlock) ? 2 : 1;
        }
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[itemIdx]);
        const QString itemSrc = m_items[itemIdx]->toMarkdown();
        if (mti) {
            // For each block, compute its source-line and check the lookup.
            QTextDocument *doc = mti->document();
            QTextBlock block = doc->begin();
            while (block.isValid()) {
                const int blockLine = srcLine + block.blockNumber();
                auto it = lineToHeadingIdx.constFind(blockLine);
                if (it != lineToHeadingIdx.constEnd())
                    m_blockToHeadingIdx.insert({itemIdx, block.blockNumber()}, *it);
                block = block.next();
            }
        }
        srcLine += int(itemSrc.count(QLatin1Char('\n')));
    }
}

int SceneCoordinator::headingAtBlock(int itemIdx, int blockNumber) const
{
    ensureHeadingMap();
    return m_blockToHeadingIdx.value({itemIdx, blockNumber}, -1);
}

int SceneCoordinator::itemIndexAt(qreal sceneY) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        QGraphicsItem *gi = m_items[i]->asGraphicsItem();
        if (!gi) continue;
        const QRectF r = gi->sceneBoundingRect();
        if (sceneY >= r.top() && sceneY <= r.bottom())
            return i;
    }
    return -1;
}

QStringList SceneCoordinator::enclosingHeadingPath(int itemIndex) const
{
    if (!m_foldingModel) return {};
    const auto &hs = m_foldingModel->headings();
    if (hs.isEmpty()) return {};

    // The most-recent heading at or before itemIndex (across all blocks).
    // Use the AST-derived map; ties broken by document order.
    int hIdx = -1;
    for (int i = 0; i <= itemIndex && i < m_items.size(); ++i) {
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[i]);
        if (!mti) continue;
        QTextBlock block = mti->document()->begin();
        while (block.isValid()) {
            const int h = headingAtBlock(i, block.blockNumber());
            if (h >= 0) hIdx = h;
            block = block.next();
        }
    }
    return hIdx >= 0 ? hs[hIdx].path : QStringList{};
}

QStringList SceneCoordinator::enclosingHeadingPathAtBlock(int itemIndex, int blockNumber) const
{
    if (!m_foldingModel) return {};
    const auto &hs = m_foldingModel->headings();
    if (hs.isEmpty()) return {};

    // Use the AST-derived map. Within the target item, stop after blockNumber
    // so headings later in the same item don't supersede the match position.
    int hIdx = -1;
    for (int i = 0; i <= itemIndex && i < m_items.size(); ++i) {
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[i]);
        if (!mti) continue;

        QTextBlock block = mti->document()->begin();
        while (block.isValid()) {
            if (i == itemIndex && block.blockNumber() > blockNumber) break;
            const int h = headingAtBlock(i, block.blockNumber());
            if (h >= 0) hIdx = h;
            block = block.next();
        }
    }

    return hIdx >= 0 ? hs[hIdx].path : QStringList{};
}

int SceneCoordinator::headingIndexForItem(int itemIndex) const
{
    if (!m_foldingModel) return -1;
    if (itemIndex < 0 || itemIndex >= m_items.size()) return -1;
    auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[itemIndex]);
    if (!mti) return -1;
    // The item is a "heading item" iff its first block is a heading.
    return headingAtBlock(itemIndex, 0);
}

void SceneCoordinator::applyFoldVisibility()
{
    if (!m_foldingModel) return;
    const auto &hs = m_foldingModel->headings();

    // Walk all items. For text items, operate at QTextBlock granularity so
    // that individual paragraphs and headings within a single MarkdownTextItem
    // can be shown/hidden independently (MarkdownSplitter does not split at
    // heading boundaries — a whole section may live in one item).
    //
    // For non-text items (tables/images), operate at the item level.
    int hIdx = -1;  // index of current enclosing heading in hs[]

    for (int itemIdx = 0; itemIdx < m_items.size(); ++itemIdx) {
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[itemIdx]);
        if (!mti) {
            // Non-text item: hide/show based on enclosing heading.
            const QStringList path = (hIdx >= 0) ? hs[hIdx].path : QStringList{};
            bool hidden = !path.isEmpty()
                && (m_foldingModel->isFolded(path)
                    || m_foldingModel->isHiddenByFold(path));
            m_items[itemIdx]->asGraphicsItem()->setVisible(!hidden);
            continue;
        }

        // Text item: walk its QTextBlocks.
        QTextDocument *doc = mti->document();
        QTextBlock block = doc->begin();
        bool anyBlockVisible = false;
        while (block.isValid()) {
            const int h = headingAtBlock(itemIdx, block.blockNumber());
            const bool isHeading = (h >= 0);
            if (isHeading) hIdx = h;

            bool hidden = false;
            if (hIdx >= 0) {
                const QStringList &path = hs[hIdx].path;
                if (isHeading) {
                    // The heading line itself stays visible unless an ANCESTOR
                    // heading is folded (isHiddenByFold checks strict prefixes).
                    hidden = m_foldingModel->isHiddenByFold(path);
                } else {
                    // Body block: hidden if the enclosing heading is folded
                    // OR any ancestor heading is folded.
                    hidden = m_foldingModel->isFolded(path)
                          || m_foldingModel->isHiddenByFold(path);
                }
            }

            mti->setBlockFolded(block.blockNumber(), hidden);
            if (!hidden) anyBlockVisible = true;
            block = block.next();
        }

        // The item stays in the scene (we never remove it); its QGraphicsItem
        // visibility tracks whether it has ANY visible content.
        m_items[itemIdx]->asGraphicsItem()->setVisible(anyBlockVisible);
    }
    repositionItems();
}

int SceneCoordinator::headingIndexAtSceneY(qreal sceneY) const
{
    for (int itemIdx = 0; itemIdx < m_items.size(); ++itemIdx) {
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[itemIdx]);
        if (!mti) continue;

        QGraphicsItem *gi = mti->asGraphicsItem();
        if (!gi) continue;
        const qreal itemSceneY = gi->scenePos().y();

        QTextBlock block = mti->document()->begin();
        while (block.isValid()) {
            const int h = headingAtBlock(itemIdx, block.blockNumber());
            if (h >= 0) {
                QTextLayout *layout = block.layout();
                qreal blockTop = itemSceneY;
                qreal blockHeight = 16.0;
                if (layout) {
                    blockTop = itemSceneY + layout->position().y();
                    if (layout->lineCount() > 0)
                        blockHeight = layout->boundingRect().height();
                }
                if (sceneY >= blockTop && sceneY < blockTop + blockHeight)
                    return h;
            }
            block = block.next();
        }
    }
    return -1;
}

qreal SceneCoordinator::headingSceneY(int headingIndex) const
{
    if (headingIndex < 0) return -1.0;

    for (int itemIdx = 0; itemIdx < m_items.size(); ++itemIdx) {
        auto *mti = dynamic_cast<MarkdownTextItem *>(m_items[itemIdx]);
        if (!mti) continue;

        QGraphicsItem *gi = mti->asGraphicsItem();
        if (!gi) continue;
        const qreal itemSceneY = gi->scenePos().y();

        QTextBlock block = mti->document()->begin();
        while (block.isValid()) {
            if (headingAtBlock(itemIdx, block.blockNumber()) == headingIndex) {
                QTextLayout *layout = block.layout();
                return layout ? itemSceneY + layout->position().y() : itemSceneY;
            }
            block = block.next();
        }
    }
    return -1.0;
}

} // namespace Markoff
