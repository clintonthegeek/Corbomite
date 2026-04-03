// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownTextItem.h"
#include "TextControl.h"

#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QStyleOptionGraphicsItem>
#include "MarkdownHighlighter.h"
#include "SourceSpan.h"
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QRegularExpression>
#include <QSyntaxHighlighter>

namespace Markoff {

MarkdownTextItem::MarkdownTextItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_document(new QTextDocument(this))
    , m_control(new TextControl(this))
{
    m_control->setDocument(m_document);
    m_control->setTextInteractionFlags(Qt::TextEditorInteraction);
    m_document->setDocumentMargin(8);

    setFlag(ItemIsFocusable);
    setFlag(ItemAcceptsInputMethod);
    setAcceptedMouseButtons(Qt::AllButtons);

    connect(m_document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, &MarkdownTextItem::updateGeometry);
    connect(m_control, &TextControl::updateRequest,
            this, [this]() { update(); });
    connect(m_control, &TextControl::textChanged,
            this, &MarkdownTextItem::textChanged);
    connect(m_control, &TextControl::cursorPositionChanged,
            this, &MarkdownTextItem::onCursorPositionChanged);
}

MarkdownTextItem::~MarkdownTextItem() = default;

void MarkdownTextItem::setPlainText(const QString &text)
{
    m_document->setPlainText(text);
    detectDecoratedRanges();
}

void MarkdownTextItem::setTextWidth(qreal width)
{
    if (qFuzzyCompare(m_width, width))
        return;
    prepareGeometryChange();
    m_width = width;
    m_document->setTextWidth(width);
}

QTextDocument *MarkdownTextItem::document() const
{
    return m_document;
}

QRectF MarkdownTextItem::boundingRect() const
{
    return {0, 0, m_width, m_document->size().height()};
}

void MarkdownTextItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem * /*option*/,
                             QWidget *widget)
{
    painter->save();
    paintDecoratedRanges(painter);
    m_control->drawContents(painter, boundingRect(), widget);
    painter->restore();
}

int MarkdownTextItem::hitTest(const QPointF &scenePos) const
{
    QPointF localPos = mapFromScene(scenePos);
    return m_document->documentLayout()->hitTest(localPos, Qt::FuzzyHit);
}

void MarkdownTextItem::setSelection(int anchorPos, int cursorPos)
{
    QTextCursor cursor(m_document);
    cursor.setPosition(anchorPos);
    cursor.setPosition(cursorPos, QTextCursor::KeepAnchor);
    m_control->setTextCursor(cursor);
}

void MarkdownTextItem::clearSelection()
{
    QTextCursor cursor = m_control->textCursor();
    cursor.clearSelection();
    m_control->setTextCursor(cursor);
}

QString MarkdownTextItem::selectedMarkdown() const
{
    QTextCursor cursor = m_control->textCursor();
    return cursor.selectedText();
}

QString MarkdownTextItem::allMarkdown() const
{
    return m_document->toPlainText();
}

QString MarkdownTextItem::toMarkdown() const
{
    return allMarkdown();
}

void MarkdownTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
    event->accept(); // Accept all buttons to hold grab for middle-click paste
}

void MarkdownTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
}

void MarkdownTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
}

void MarkdownTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    m_control->processEvent(event);
}

void MarkdownTextItem::keyPressEvent(QKeyEvent *event)
{
    // Check for cursor-at-boundary before forwarding
    QTextCursor cursor = m_control->textCursor();
    bool atStart = cursor.atStart();
    bool atEnd = cursor.atEnd();

    m_control->processEvent(event);

    // If cursor didn't move for arrow keys, we're at a boundary
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Home) {
        if (atStart && m_control->textCursor().atStart())
            emit cursorAtBoundary(Qt::TopEdge);
    } else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_End) {
        if (atEnd && m_control->textCursor().atEnd())
            emit cursorAtBoundary(Qt::BottomEdge);
    }
}

void MarkdownTextItem::inputMethodEvent(QInputMethodEvent *event)
{
    m_control->processEvent(event);
}

QVariant MarkdownTextItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    return m_control->inputMethodQuery(query, QVariant());
}

void MarkdownTextItem::focusInEvent(QFocusEvent *event)
{
    m_control->setFocus(true, event->reason());
    QGraphicsObject::focusInEvent(event);
}

void MarkdownTextItem::focusOutEvent(QFocusEvent *event)
{
    m_control->setFocus(false, event->reason());
    QGraphicsObject::focusOutEvent(event);
}

void MarkdownTextItem::onCursorPositionChanged()
{
    if (m_snappingCursor)
        return;

    // 1. Snap cursor past hidden delimiters
    snapCursorPastDelimiters();

    // 2. Notify highlighter of cursor position (shows/hides delimiters)
    auto *hl = qobject_cast<MarkdownHighlighter *>(
        m_document->findChild<QSyntaxHighlighter *>());
    if (hl) {
        QTextCursor cursor = m_control->textCursor();
        hl->setCursorPosition(cursor.block().blockNumber(),
                              cursor.positionInBlock());
    }
}

void MarkdownTextItem::snapCursorPastDelimiters()
{
    auto *hl = qobject_cast<MarkdownHighlighter *>(
        m_document->findChild<QSyntaxHighlighter *>());
    if (!hl || hl->mode() != MarkdownHighlighter::Mode::LivePreview)
        return;

    QTextCursor cursor = m_control->textCursor();
    if (cursor.hasSelection())
        return; // don't snap during selection

    int pos = cursor.position();

    for (const SourceSpan &span : hl->spans()) {
        if (!span.isDelimiter)
            continue;
        int spanStart = span.charOffset;
        int spanEnd = span.charOffset + span.charLength;
        if (pos >= spanStart && pos < spanEnd) {
            // Cursor is inside a hidden delimiter — snap to end
            m_snappingCursor = true;
            cursor.setPosition(spanEnd);
            m_control->setTextCursor(cursor);
            m_snappingCursor = false;
            return;
        }
    }
}

void MarkdownTextItem::detectDecoratedRanges()
{
    m_decoratedRanges.clear();

    // Detect fenced code blocks: ``` ... ```
    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        const QString text = block.text().trimmed();
        if (text.startsWith(QStringLiteral("```"))) {
            int firstBlockNum = block.blockNumber();
            QString lang = text.mid(3).trimmed();

            block = block.next();
            int lastBlockNum = firstBlockNum;
            while (block.isValid()) {
                if (block.text().trimmed().startsWith(QStringLiteral("```"))) {
                    lastBlockNum = block.blockNumber();
                    break;
                }
                lastBlockNum = block.blockNumber();
                block = block.next();
            }
            if (block.isValid() && block.text().trimmed().startsWith(QStringLiteral("```"))) {
                DecoratedRange dr;
                dr.type = DecoratedRange::CodeBlock;
                dr.firstBlock = firstBlockNum;
                dr.lastBlock = lastBlockNum;
                dr.language = lang;
                m_decoratedRanges.append(dr);
            }
        }
        if (block.isValid()) block = block.next();
    }

    // Detect callout blocks (> [!type] ...)
    block = m_document->begin();
    static const QRegularExpression calloutRe(
        QStringLiteral(R"(^>\s*\[!(\w+)\]([+-])?\s*(.*)?$)"));

    while (block.isValid()) {
        bool inCodeBlock = false;
        for (const auto &dr : m_decoratedRanges) {
            if (dr.type == DecoratedRange::CodeBlock
                && block.blockNumber() >= dr.firstBlock
                && block.blockNumber() <= dr.lastBlock) {
                inCodeBlock = true;
                break;
            }
        }
        if (inCodeBlock) { block = block.next(); continue; }

        auto match = calloutRe.match(block.text());
        if (match.hasMatch()) {
            int firstBlockNum = block.blockNumber();
            QString type = match.captured(1).toLower();
            QString title = match.captured(3).trimmed();

            QTextBlock bodyBlock = block.next();
            int lastBlockNum = firstBlockNum;
            while (bodyBlock.isValid() && bodyBlock.text().startsWith(QLatin1Char('>'))) {
                lastBlockNum = bodyBlock.blockNumber();
                bodyBlock = bodyBlock.next();
            }

            DecoratedRange dr;
            dr.type = DecoratedRange::Callout;
            dr.firstBlock = firstBlockNum;
            dr.lastBlock = lastBlockNum;
            dr.calloutType = type;
            dr.calloutTitle = title.isEmpty()
                ? type.at(0).toUpper() + type.mid(1) : title;
            dr.calloutColor = DecoratedRange::colorForCalloutType(type);
            m_decoratedRanges.append(dr);

            block = bodyBlock;
            continue;
        }
        block = block.next();
    }
}

void MarkdownTextItem::paintDecoratedRanges(QPainter *painter)
{
    if (m_decoratedRanges.isEmpty())
        return;

    QAbstractTextDocumentLayout *layout = m_document->documentLayout();
    qreal margin = m_document->documentMargin();

    for (const DecoratedRange &dr : m_decoratedRanges) {
        QTextBlock firstBlock = m_document->findBlockByNumber(dr.firstBlock);
        if (!firstBlock.isValid()) continue;

        QRectF firstBR = layout->blockBoundingRect(firstBlock);
        qreal rangeTop = firstBR.top();
        qreal rangeHeight = 0;
        QTextBlock b = firstBlock;
        for (int i = dr.firstBlock; i <= dr.lastBlock && b.isValid(); ++i, b = b.next())
            rangeHeight += layout->blockBoundingRect(b).height();

        QRectF bgRect(margin - 4, rangeTop, m_width - margin * 2 + 8, rangeHeight);

        if (dr.type == DecoratedRange::CodeBlock) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xf5, 0xf5, 0xf5));
            painter->setRenderHint(QPainter::Antialiasing);
            painter->drawRoundedRect(bgRect, 4, 4);
            painter->setPen(QPen(QColor(0xe0, 0xe0, 0xe0), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            if (!dr.language.isEmpty()) {
                QFont labelFont = painter->font();
                labelFont.setPointSize(qMax(8, labelFont.pointSize() - 2));
                painter->setFont(labelFont);
                painter->setPen(QColor(0x9e, 0x9e, 0x9e));
                QRectF labelRect(bgRect.right() - 80, bgRect.top() + 2, 72, 16);
                painter->drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter,
                                  dr.language);
            }
        } else if (dr.type == DecoratedRange::Callout) {
            QColor bg = dr.calloutColor;
            bg.setAlpha(20);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bg);
            painter->setRenderHint(QPainter::Antialiasing);
            painter->drawRoundedRect(bgRect, 4, 4);
            painter->setBrush(dr.calloutColor);
            painter->drawRoundedRect(
                QRectF(bgRect.left(), bgRect.top(), 4, bgRect.height()), 2, 2);
        }
    }
}

void MarkdownTextItem::updateGeometry()
{
    prepareGeometryChange();
}

} // namespace Markoff
