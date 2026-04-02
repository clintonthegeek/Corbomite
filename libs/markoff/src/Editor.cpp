// SPDX-License-Identifier: GPL-3.0-or-later
// Forked from Qt's QPlainTextEdit / QPlainTextDocumentLayout
// Original: Copyright (C) The Qt Company Ltd. (GPL-2.0-only OR GPL-3.0-only)

#include "Editor_p.h"
#include "TextControl.h"
#include "TextControl_p.h"
#include "MarkdownHighlighter.h"
#include "MarkoffBlockData.h"
#include "SourceSpan.h"
#include "DocumentBuilder_p.h"

#include <QRegularExpression>
#include <QTimer>
#include "markoff/Document.h"
#include "markoff/Renderer.h"
#include "markoff/RenderSettings.h"

#include <qfont.h>
#include <qpainter.h>
#include <qevent.h>
#include <qdebug.h>
#if QT_CONFIG(draganddrop)
#include <qdrag.h>
#endif
#include <qclipboard.h>
#include <qmath.h>
#include <qmenu.h>
#include <qstyle.h>
#include <qapplication.h>
#include <qtextdocument.h>
#include <qtextlist.h>
#include <qaccessible.h>
#include <qtextformat.h>
#include <qdatetime.h>
#include <limits.h>
#include <qtexttable.h>
#include <qvariant.h>
#include <qscrollbar.h>
#include <qinputmethod.h>

namespace Markoff {

// ============================================================================
// PlainTextDocumentLayout — forked from QPlainTextDocumentLayout
// ============================================================================

/// Layout data for a QTextTable, stored via QTextFrame::setLayoutData().
/// Simplified from Qt's QTextTableData — uses qreal instead of QFixed,
/// no pagination, no border-collapse, no CSS box model.
struct TableLayoutData : public QTextFrameLayoutData
{
    qreal cellPadding = 4.0;
    qreal cellSpacing = 0.0;

    QList<qreal> widths;           // column widths
    QList<qreal> heights;          // row heights
    QList<qreal> columnPositions;  // x position of each column
    QList<qreal> rowPositions;     // y position of each row
    QList<qreal> cellVerticalOffsets; // vertical alignment offsets per cell

    qreal contentsWidth = 0;
    qreal tableWidth = 0;
    qreal tableHeight = 0;

    bool dirty = true;

    qreal cellWidth(int column, int colspan) const
    {
        return columnPositions.at(column + colspan - 1) + widths.at(column + colspan - 1)
               - columnPositions.at(column);
    }

    void calcRowPosition(int row)
    {
        if (row > 0)
            rowPositions[row] = rowPositions.at(row - 1) + heights.at(row - 1) + cellSpacing;
    }

    QRectF cellRect(const QTextTableCell &cell) const
    {
        const int row = cell.row();
        const int rowSpan = cell.rowSpan();
        const int column = cell.column();
        const int colSpan = cell.columnSpan();
        return QRectF(columnPositions.at(column),
                      rowPositions.at(row),
                      cellWidth(column, colSpan),
                      rowPositions.at(row + rowSpan - 1) + heights.at(row + rowSpan - 1) - rowPositions.at(row));
    }
};

static QTextTable *tableForBlock(const QTextBlock &block)
{
    // QTextCursor is lightweight — this is cheap
    QTextCursor cursor(block);
    return cursor.currentTable();
}

static bool isFirstTableBlock(const QTextBlock &block, QTextTable *table)
{
    if (!table) return false;
    QTextTableCell firstCell = table->cellAt(0, 0);
    return firstCell.firstCursorPosition().block() == block;
}

static TableLayoutData *tableLayoutData(QTextTable *table)
{
    auto *data = static_cast<TableLayoutData *>(table->layoutData());
    if (!data) {
        data = new TableLayoutData;
        table->setLayoutData(data);
    }
    return data;
}

// Private data for PlainTextDocumentLayout (replaces QPlainTextDocumentLayoutPrivate)
struct PlainTextDocumentLayoutPrivate {
    qreal width = 0;
    qreal maximumWidth = 0;
    int maximumWidthBlockNumber = 0;
    int blockCount = 1;
    Editor::Private *mainViewPrivate = nullptr;
    bool blockUpdate = false;
    bool blockDocumentSizeChanged = false;
    int cursorWidth = 1;
    int textLayoutFlags = 0;
    mutable bool inBlockBoundingRect = false;  // recursion guard for table detection
    bool inTableLayout = false;  // guard: don't re-dirty table during layoutTable()

    void layoutBlock(QTextDocument *doc, const QTextBlock &block);
    qreal blockWidth(const QTextBlock &block);
    void relayout(QTextDocument *doc);
};

class PlainTextDocumentLayout : public QAbstractTextDocumentLayout {
    Q_OBJECT
public:
    PlainTextDocumentLayout(QTextDocument *document);
    ~PlainTextDocumentLayout() override;

    void draw(QPainter *, const PaintContext &) override;
    int hitTest(const QPointF &, Qt::HitTestAccuracy) const override;
    int pageCount() const override;
    QSizeF documentSize() const override;
    QRectF frameBoundingRect(QTextFrame *) const override;
    QRectF blockBoundingRect(const QTextBlock &block) const override;

    void ensureBlockLayout(const QTextBlock &block) const;

    void setCursorWidth(int width);
    int cursorWidth() const;

    void requestUpdate();
    void setTextWidth(qreal newWidth);
    qreal textWidth() const;

    PlainTextDocumentLayoutPrivate *priv() { return &d; }
    const PlainTextDocumentLayoutPrivate *priv() const { return &d; }

protected:
    void documentChanged(int from, int charsRemoved, int charsAdded) override;

private:
    void layoutBlock(const QTextBlock &block);
    qreal blockWidth(const QTextBlock &block);

    struct CellLayoutResult {
        qreal height = 0;
        qreal minimumWidth = 0;
        qreal maximumWidth = 0;
    };
    CellLayoutResult layoutCellContent(QTextTable *table, const QTextTableCell &cell, qreal width);
    void layoutTable(QTextTable *table);

    PlainTextDocumentLayoutPrivate d;

    friend class Editor;
    friend struct Editor::Private;
};


PlainTextDocumentLayout::PlainTextDocumentLayout(QTextDocument *document)
    : QAbstractTextDocumentLayout(document)
{
    // Set cursorWidth as a dynamic property so TextControl::cursorWidth()
    // can read it via property("cursorWidth")
    setProperty("cursorWidth", d.cursorWidth);
}

PlainTextDocumentLayout::~PlainTextDocumentLayout() {}

void PlainTextDocumentLayout::draw(QPainter *, const PaintContext &)
{
}

int PlainTextDocumentLayout::hitTest(const QPointF &, Qt::HitTestAccuracy) const
{
    return -1;
}

int PlainTextDocumentLayout::pageCount() const
{ return 1; }

QSizeF PlainTextDocumentLayout::documentSize() const
{
    return QSizeF(d.maximumWidth, document()->lineCount());
}

QRectF PlainTextDocumentLayout::frameBoundingRect(QTextFrame *) const
{
    return QRectF(0, 0, qMax(d.width, d.maximumWidth), qreal(INT_MAX));
}

QRectF PlainTextDocumentLayout::blockBoundingRect(const QTextBlock &block) const
{
    if (!block.isValid()) { return QRectF(); }

    // Table detection creates QTextCursor objects, which can trigger
    // Qt to call back into blockBoundingRect() — guard against recursion.
    if (!d.inBlockBoundingRect) {
        d.inBlockBoundingRect = true;
        QTextTable *table = tableForBlock(block);
        if (table) {
            if (isFirstTableBlock(block, table)) {
                TableLayoutData *td = tableLayoutData(table);
                if (td->dirty)
                    const_cast<PlainTextDocumentLayout*>(this)->layoutTable(table);
                d.inBlockBoundingRect = false;
                return QRectF(0, 0, td->tableWidth, td->tableHeight);
            } else {
                // Non-first block inside table: zero height
                d.inBlockBoundingRect = false;
                return QRectF(0, 0, 0, 0);
            }
        }
        d.inBlockBoundingRect = false;
    }

    QTextLayout *tl = block.layout();
    if (!tl->lineCount())
        const_cast<PlainTextDocumentLayout*>(this)->layoutBlock(block);
    QRectF br;
    if (block.isVisible()) {
        br = QRectF(QPointF(0, 0), tl->boundingRect().bottomRight());
        if (tl->lineCount() == 1)
            br.setWidth(qMax(br.width(), tl->lineAt(0).naturalTextWidth()));
        qreal margin = document()->documentMargin();
        br.adjust(0, 0, margin, 0);
        if (!block.next().isValid())
            br.adjust(0, 0, 0, margin);

        // Variable block heights for live preview: if this block has a
        // rendered height (from MarkoffBlockData), use it instead of the
        // text layout height. This makes rendered blocks (which may be
        // Variable block heights for rendered content
        auto *data = dynamic_cast<MarkoffBlockData *>(block.userData());
        if (data && data->displayMode == MarkoffBlockData::Rendered
            && data->renderedHeight > 0) {
            br.setHeight(data->renderedHeight);
        }
    }
    return br;
}

void PlainTextDocumentLayout::ensureBlockLayout(const QTextBlock &block) const
{
    if (!block.isValid())
        return;
    QTextLayout *tl = block.layout();
    if (!tl->lineCount())
        const_cast<PlainTextDocumentLayout*>(this)->layoutBlock(block);
}

void PlainTextDocumentLayout::setCursorWidth(int width)
{
    d.cursorWidth = width;
    // Also set as a dynamic property so TextControl::cursorWidth() can read it
    setProperty("cursorWidth", width);
}

int PlainTextDocumentLayout::cursorWidth() const
{
    return d.cursorWidth;
}

void PlainTextDocumentLayout::requestUpdate()
{
    emit update(QRectF(0., -document()->documentMargin(), 1000000000., 1000000000.));
}

void PlainTextDocumentLayout::setTextWidth(qreal newWidth)
{
    d.width = d.maximumWidth = newWidth;
    d.relayout(document());
}

qreal PlainTextDocumentLayout::textWidth() const
{
    return d.width;
}

void PlainTextDocumentLayoutPrivate::relayout(QTextDocument *doc)
{
    QTextBlock block = doc->firstBlock();
    while (block.isValid()) {
        block.layout()->clearLayout();
        block.setLineCount(block.isVisible() ? 1 : 0);
        block = block.next();
    }
    // Signal update via the layout's document; the layout will emit update()
    // when documentChanged is called, or we can request it explicitly.
}

void PlainTextDocumentLayout::documentChanged(int from, int charsRemoved, int charsAdded)
{
    QTextDocument *doc = document();
    int newBlockCount = doc->blockCount();
    int charsChanged = charsRemoved + charsAdded;

    QTextBlock changeStartBlock = doc->findBlock(from);

    // If we're inside layoutTable(), block layout changes trigger
    // documentChanged but we must NOT re-dirty the table.
    if (d.inTableLayout) {
        d.blockCount = newBlockCount;
        return;
    }

    // If the change is inside a table, mark the table layout dirty
    // and invalidate the whole viewport
    QTextTable *table = tableForBlock(changeStartBlock);
    if (table) {
        TableLayoutData *td = tableLayoutData(table);
        td->dirty = true;
        d.blockCount = newBlockCount;
        emit update(QRectF(0., -doc->documentMargin(), 1000000000., 1000000000.));
        return;
    }

    QTextBlock changeEndBlock = doc->findBlock(qMax(0, from + charsChanged - 1));
    bool blockVisibilityChanged = false;

    if (changeStartBlock == changeEndBlock && newBlockCount == d.blockCount) {
        QTextBlock block = changeStartBlock;
        if (block.isValid() && block.length()) {
            QRectF oldBr = blockBoundingRect(block);
            layoutBlock(block);
            QRectF newBr = blockBoundingRect(block);
            if (newBr.height() == oldBr.height()) {
                if (!d.blockUpdate)
                    emit updateBlock(block);
                return;
            }
        }
    } else {
        QTextBlock block = changeStartBlock;
        do {
            block.clearLayout();
            if (block.isVisible()
                    ? (block.lineCount() == 0)
                    : (block.lineCount() > 0)) {
                blockVisibilityChanged = true;
                block.setLineCount(block.isVisible() ? 1 : 0);
            }
            if (block == changeEndBlock)
                break;
            block = block.next();
        } while(block.isValid());
    }

    if (newBlockCount != d.blockCount || blockVisibilityChanged) {
        int changeEnd = changeEndBlock.blockNumber();
        int blockDiff = newBlockCount - d.blockCount;
        int oldChangeEnd = changeEnd - blockDiff;

        if (d.maximumWidthBlockNumber > oldChangeEnd)
            d.maximumWidthBlockNumber += blockDiff;

        d.blockCount = newBlockCount;
        if (d.blockCount == 1)
            d.maximumWidth = d.blockWidth(doc->firstBlock());

        if (!d.blockDocumentSizeChanged)
            emit documentSizeChanged(documentSize());

        if (blockDiff == 1 && changeEnd == newBlockCount -1 ) {
            if (!d.blockUpdate) {
                QTextBlock b = changeStartBlock;
                for(;;) {
                    emit updateBlock(b);
                    if (b == changeEndBlock)
                        break;
                    b = b.next();
                }
            }
            return;
        }
    }

    if (!d.blockUpdate)
        emit update(QRectF(0., -doc->documentMargin(), 1000000000., 1000000000.));
}

void PlainTextDocumentLayout::layoutBlock(const QTextBlock &block)
{
    d.layoutBlock(document(), block);
}

void PlainTextDocumentLayoutPrivate::layoutBlock(QTextDocument *doc, const QTextBlock &block)
{
    qreal margin = doc->documentMargin();
    qreal blockMaximumWidth = 0;

    qreal height = 0;
    QTextLayout *tl = block.layout();
    QTextOption option = doc->defaultTextOption();
    tl->setTextOption(option);

    int extraMargin = 0;
    if (option.flags() & QTextOption::AddSpaceForLineAndParagraphSeparators) {
        QFontMetrics fm(block.charFormat().font());
        extraMargin += fm.horizontalAdvance(QChar(0x21B5));
    }
    tl->beginLayout();
    qreal availableWidth = width;
    if (availableWidth <= 0) {
        availableWidth = qreal(INT_MAX);
    }
    availableWidth -= 2*margin + extraMargin;
    while (1) {
        QTextLine line = tl->createLine();
        if (!line.isValid())
            break;
        line.setLeadingIncluded(true);
        line.setLineWidth(availableWidth);
        line.setPosition(QPointF(margin, height));
        height += line.height();
        if (line.leading() < 0)
            height += qCeil(line.leading());
        blockMaximumWidth = qMax(blockMaximumWidth, line.naturalTextWidth() + 2*margin);
    }
    tl->endLayout();

    int previousLineCount = doc->lineCount();
    const_cast<QTextBlock&>(block).setLineCount(block.isVisible() ? tl->lineCount() : 0);
    int lineCount = doc->lineCount();

    bool emitDocumentSizeChanged = previousLineCount != lineCount;
    if (blockMaximumWidth > maximumWidth) {
        maximumWidth = blockMaximumWidth;
        maximumWidthBlockNumber = block.blockNumber();
        emitDocumentSizeChanged = true;
    } else if (block.blockNumber() == maximumWidthBlockNumber && blockMaximumWidth < maximumWidth) {
        QTextBlock b = doc->firstBlock();
        maximumWidth = 0;
        QTextBlock maximumBlock;
        while (b.isValid()) {
            qreal bw = blockWidth(b);
            if (bw > maximumWidth) {
                maximumWidth = bw;
                maximumBlock = b;
            }
            b = b.next();
        }
        if (maximumBlock.isValid()) {
            maximumWidthBlockNumber = maximumBlock.blockNumber();
            emitDocumentSizeChanged = true;
        }
    }
    if (emitDocumentSizeChanged && !blockDocumentSizeChanged) {
        auto *layout = qobject_cast<PlainTextDocumentLayout*>(doc->documentLayout());
        if (layout)
            emit layout->documentSizeChanged(layout->documentSize());
    }
}

qreal PlainTextDocumentLayout::blockWidth(const QTextBlock &block)
{
    return d.blockWidth(block);
}

qreal PlainTextDocumentLayoutPrivate::blockWidth(const QTextBlock &block)
{
    QTextLayout *layout = block.layout();
    if (!layout->lineCount())
        return 0;
    qreal bw = 0;
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        bw = qMax(line.naturalTextWidth() + 8, bw);
    }
    return bw;
}


// ============================================================================
// Table layout — harvested from Qt's QTextDocumentLayout, simplified
// ============================================================================

PlainTextDocumentLayout::CellLayoutResult
PlainTextDocumentLayout::layoutCellContent(
    QTextTable * /*table*/, const QTextTableCell &cell, qreal width)
{
    CellLayoutResult result;
    QTextBlock block = cell.firstCursorPosition().block();
    QTextBlock lastBlock = cell.lastCursorPosition().block();

    while (block.isValid()) {
        QTextLayout *tl = block.layout();
        QTextOption option = document()->defaultTextOption();
        option.setTextDirection(tl->textOption().textDirection());
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

        tl->setTextOption(option);
        tl->setCacheEnabled(true);

        const qreal availWidth = qMax(width, qreal(0));
        tl->beginLayout();
        qreal textHeight = 0;
        qreal maxLineWidth = 0;
        while (true) {
            QTextLine line = tl->createLine();
            if (!line.isValid())
                break;
            line.setLeadingIncluded(true);
            line.setLineWidth(availWidth);
            line.setPosition(QPointF(0, textHeight));
            textHeight += line.height();
            maxLineWidth = qMax(maxLineWidth, line.naturalTextWidth());
        }
        tl->endLayout();

        result.height += textHeight;
        result.maximumWidth = qMax(result.maximumWidth, maxLineWidth);
        // minimumWidth() on QTextLayout returns the width of the longest
        // word — the smallest width that won't break mid-word.
        result.minimumWidth = qMax(result.minimumWidth, tl->minimumWidth());

        if (block == lastBlock)
            break;
        block = block.next();
    }

    return result;
}

void PlainTextDocumentLayout::layoutTable(QTextTable *table)
{
    d.inTableLayout = true;
    TableLayoutData *td = tableLayoutData(table);
    const int rows = table->rows();
    const int cols = table->columns();
    const QTextTableFormat fmt = table->format();

    td->cellPadding = fmt.cellPadding();
    td->cellSpacing = fmt.cellSpacing();

    const qreal availableWidth = d.width > 0 ? d.width : 500.0;

    // Column width constraints from the table format
    QList<QTextLength> constraints = fmt.columnWidthConstraints();
    constraints.resize(cols);

    // Step 1: Compute min/max widths for each column
    td->widths.resize(cols);
    td->widths.fill(0);
    QList<qreal> minWidths(cols, 1.0);
    QList<qreal> maxWidths(cols, 0.0);

    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
            QTextTableCell cell = table->cellAt(r, c);
            if (cell.column() != c) continue; // skip spanned
            int cspan = cell.columnSpan();

            qreal padding = td->cellPadding * 2;
            CellLayoutResult lr = layoutCellContent(table, cell, 10000.0);

            qreal minW = (lr.minimumWidth + padding) / cspan;
            qreal maxW = (lr.maximumWidth + padding) / cspan;

            for (int n = 0; n < cspan; ++n) {
                minWidths[c + n] = qMax(minWidths[c + n], minW);
                maxWidths[c + n] = qMax(maxWidths[c + n], maxW);
            }
        }
    }

    // Step 2: Distribute widths
    qreal totalAvailable = availableWidth - (cols + 1) * td->cellSpacing - cols * td->cellPadding * 2;
    qreal totalMin = 0;
    for (int c = 0; c < cols; ++c) totalMin += minWidths[c];

    if (totalMin >= totalAvailable) {
        // Minimum widths exceed available — just use minimums
        for (int c = 0; c < cols; ++c)
            td->widths[c] = minWidths[c];
    } else {
        // Distribute extra space proportionally to max-min gap
        qreal extra = totalAvailable - totalMin;
        qreal totalGap = 0;
        for (int c = 0; c < cols; ++c)
            totalGap += qMax(maxWidths[c] - minWidths[c], qreal(0));

        for (int c = 0; c < cols; ++c) {
            if (totalGap > 0) {
                qreal gap = qMax(maxWidths[c] - minWidths[c], qreal(0));
                td->widths[c] = minWidths[c] + extra * gap / totalGap;
            } else {
                td->widths[c] = minWidths[c] + extra / cols;
            }
        }
    }

    // Step 3: Column positions (prefix sum)
    td->columnPositions.resize(cols);
    td->columnPositions[0] = td->cellSpacing;
    for (int c = 1; c < cols; ++c)
        td->columnPositions[c] = td->columnPositions[c-1] + td->widths[c-1] + td->cellSpacing;

    td->contentsWidth = td->columnPositions.last() + td->widths.last() + td->cellSpacing;

    // Step 4: Row heights
    td->heights.resize(rows);
    td->heights.fill(0);
    td->rowPositions.resize(rows);
    td->rowPositions[0] = td->cellSpacing;

    QList<qreal> heightToDistribute(cols, 0);

    for (int r = 0; r < rows; ++r) {
        td->calcRowPosition(r);

        for (int c = 0; c < cols; ++c) {
            QTextTableCell cell = table->cellAt(r, c);
            if (cell.column() != c) continue; // skip spanned

            int rspan = cell.rowSpan();
            int cspan = cell.columnSpan();

            if (rspan > 1 && cell.row() != r) {
                // This cell started in an earlier row; only add remaining height at last row
                if (cell.row() + rspan - 1 == r)
                    td->heights[r] = qMax(td->heights[r], heightToDistribute[c]);
                continue;
            }

            qreal cellW = td->cellWidth(c, cspan) - td->cellPadding * 2;
            CellLayoutResult lr = layoutCellContent(table, cell, cellW);
            qreal height = lr.height + td->cellPadding * 2;

            if (rspan > 1)
                heightToDistribute[c] = height;
            else
                td->heights[r] = qMax(td->heights[r], height);
        }

        // Subtract this row's contribution from pending row-span heights
        qreal effectiveHeight = td->heights[r] + td->cellSpacing;
        for (int c = 0; c < cols; ++c)
            heightToDistribute[c] = qMax(heightToDistribute[c] - effectiveHeight, qreal(0));
    }

    // Step 5: Vertical alignment offsets (all top-aligned for now)
    td->cellVerticalOffsets.resize(rows * cols);
    td->cellVerticalOffsets.fill(0);

    // Final size
    td->tableWidth = td->contentsWidth;
    td->tableHeight = td->rowPositions.last() + td->heights.last() + td->cellSpacing;
    td->dirty = false;
    d.inTableLayout = false;
}

// ============================================================================
// EditorControl — replaces QPlainTextEditControl
// ============================================================================

class EditorControl : public TextControl {
    Q_OBJECT
public:
    EditorControl(Editor *parent);

    QMimeData *createMimeDataFromSelection() const override;
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;
    int hitTest(const QPointF &point, Qt::HitTestAccuracy = Qt::FuzzyHit) const override;
    QRectF blockBoundingRect(const QTextBlock &block) const override;
    QString anchorAt(const QPointF &pos) const override;
    QRectF cursorRect(const QTextCursor &cursor) const {
        QRectF r = TextControl::cursorRect(cursor);
        r.setLeft(qMax(r.left(), (qreal) 0.));
        return r;
    }
    QRectF cursorRect() { return cursorRect(textCursor()); }
    void ensureCursorVisible() override {
        textEdit->ensureCursorVisible();
        emit microFocusChanged();
    }

    QVariant loadResource(int /*type*/, const QUrl &/*name*/) { return QVariant(); }

    Editor *textEdit;
    int topBlock = 0;
    QTextBlock firstVisibleBlock() const;
};

EditorControl::EditorControl(Editor *parent)
    : TextControl(parent), textEdit(parent)
{
    setAcceptRichText(false);
}

QMimeData *EditorControl::createMimeDataFromSelection() const {
    return TextControl::createMimeDataFromSelection();
}

bool EditorControl::canInsertFromMimeData(const QMimeData *source) const {
    return TextControl::canInsertFromMimeData(source);
}

void EditorControl::insertFromMimeData(const QMimeData *source) {
    TextControl::insertFromMimeData(source);
}

QTextBlock EditorControl::firstVisibleBlock() const
{
    // Pixel-based scrolling: find the block whose cumulative Y position
    // is at or just before the scroll offset.
    qreal scrollY = textEdit->verticalScrollBar()->value();
    qreal margin = document()->documentMargin();
    PlainTextDocumentLayout *dl = qobject_cast<PlainTextDocumentLayout*>(document()->documentLayout());
    Q_ASSERT(dl);

    qreal y = margin;
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        qreal h = dl->blockBoundingRect(block).height();
        if (y + h > scrollY)
            return block;
        y += h;
        block = block.next();
    }
    return document()->lastBlock();
}

int EditorControl::hitTest(const QPointF &point, Qt::HitTestAccuracy) const {
    // Pixel-based: point.y() is relative to the viewport.
    // Convert to document coordinates by adding scroll offset.
    qreal docY = point.y() + textEdit->verticalScrollBar()->value();

    PlainTextDocumentLayout *documentLayout = qobject_cast<PlainTextDocumentLayout*>(document()->documentLayout());
    Q_ASSERT(documentLayout);

    // Find the block at this document Y position
    qreal y = document()->documentMargin();
    QTextBlock currentBlock = document()->begin();
    while (currentBlock.isValid()) {
        qreal h = documentLayout->blockBoundingRect(currentBlock).height();
        if (y + h > docY)
            break;
        y += h;
        currentBlock = currentBlock.next();
    }

    if (!currentBlock.isValid())
        return -1;
    QTextLayout *layout = currentBlock.layout();
    int off = 0;
    QPointF pos(point.x(), docY - y);  // pos relative to block top
    for (int i = 0; i < layout->lineCount(); ++i) {
        QTextLine line = layout->lineAt(i);
        const QRectF lr = line.naturalTextRect();
        if (lr.top() > pos.y()) {
            off = qMin(off, line.textStart());
        } else if (lr.bottom() <= pos.y()) {
            off = qMax(off, line.textStart() + line.textLength());
        } else {
            off = line.xToCursor(pos.x(), overwriteMode() ?
                                 QTextLine::CursorOnCharacter : QTextLine::CursorBetweenCharacters);
            break;
        }
    }

    return currentBlock.position() + off;
}

QRectF EditorControl::blockBoundingRect(const QTextBlock &block) const {
    if (!block.isValid())
        return QRectF();
    PlainTextDocumentLayout *dl = qobject_cast<PlainTextDocumentLayout*>(document()->documentLayout());
    Q_ASSERT(dl);
    QRectF r = dl->blockBoundingRect(block);
    // Compute block's pixel Y by summing heights of preceding blocks
    qreal blockY = document()->documentMargin();
    QTextBlock b = document()->begin();
    while (b.isValid() && b != block) {
        blockY += dl->blockBoundingRect(b).height();
        b = b.next();
    }
    qreal scrollY = textEdit->verticalScrollBar()->value();
    r.translate(0, blockY - scrollY);
    return r;
}

QString EditorControl::anchorAt(const QPointF &pos) const
{
    Q_UNUSED(pos);
    return QString();
}


// Deferred definition: EditorControl is now complete
void Editor::Private::sendControlEvent(QEvent *e)
{
    control->processEvent(e, QPointF(horizontalOffset(), verticalOffset()), q->viewport());
}

// ============================================================================
// Editor::Private
// ============================================================================

void Editor::Private::init(const QString &txt)
{
    control = new EditorControl(q);

    QTextDocument *doc = new QTextDocument(control);
    QAbstractTextDocumentLayout *layout = new PlainTextDocumentLayout(doc);
    doc->setDocumentLayout(layout);
    control->setDocument(doc);

    highlighter = new MarkdownHighlighter(doc);

    control->setPalette(q->palette());

    QObject::connect(q->verticalScrollBar(), &QAbstractSlider::actionTriggered,
                     q, [this](int action) { verticalScrollbarActionTriggered(action); });
    QObject::connect(control, &TextControl::microFocusChanged, q,
                     [this]() { q->updateMicroFocus(); });
    QObject::connect(control, &TextControl::documentSizeChanged, q,
                     [this](const QSizeF &) { adjustScrollbars(); });
    QObject::connect(control, &TextControl::updateRequest, q,
                     [this](const QRectF &rect) { repaintContents(rect); });
    QObject::connect(control, &TextControl::textChanged, q, &Editor::textChanged);
    QObject::connect(control, &TextControl::textChanged, q, [this]() { q->updateMicroFocus(); });

    // Live preview: re-parse on text changes.
    // Deferred with QTimer::singleShot(0) so the reparse runs AFTER the
    // current event fully completes. This handles multi-step operations
    // (drag-drop = delete + insert, undo, etc.) that fire textChanged on
    // intermediate states. The reparse always sees the final document.
    QObject::connect(control, &TextControl::textChanged, q, [this]() {
        if (mode == Editor::Mode::LivePreview && !needsReparse && !inReparse) {
            needsReparse = true;
            QTimer::singleShot(0, q, [this]() {
                if (!needsReparse) return;
                needsReparse = false;
                inReparse = true;
                highlighter->setCursorPosition(
                    control->textCursor().block().blockNumber(),
                    control->textCursor().positionInBlock());
                reparseDocument();  // revert tables, parse, highlight setup, block formats
                highlighter->rehighlight();
                updateBlockDisplayModes();
                // Convert tables as the VERY LAST step — after all block
                // iteration (rehighlight, updateBlockDisplayModes, applyBlockFormats)
                // is done. These functions walk all blocks and would corrupt
                // QTextTable cell structure if tables existed during their run.
                convertTables();
                inReparse = false;
                inReparse = false;
            });
        }
    });

    // Live preview: update display modes and highlighter on cursor movement.
    // Suppressed during mouse drag to avoid per-pixel rehighlighting.
    QObject::connect(control, &TextControl::cursorPositionChanged, q, [this]() {
        if (mouseDragging)
            return;  // defer until mouse release

        if (mode == Editor::Mode::LivePreview) {
            int cursorBlockNum = control->textCursor().block().blockNumber();
            highlighter->setCursorPosition(cursorBlockNum, control->textCursor().positionInBlock());
        }
        updateBlockDisplayModes();
    });

    doc->setTextWidth(-1);
    doc->documentLayout()->setPaintDevice(q->viewport());
    doc->setDefaultFont(q->font());

    if (!txt.isEmpty())
        control->setPlainText(txt);

    q->horizontalScrollBar()->setSingleStep(20);
    q->verticalScrollBar()->setSingleStep(1);

    q->viewport()->setBackgroundRole(QPalette::Base);
    q->viewport()->setMouseTracking(true);  // fire mouseMoveEvent without button pressed
    q->setAcceptDrops(true);
    q->setFocusPolicy(Qt::StrongFocus);
    q->setAttribute(Qt::WA_KeyCompression);
    q->setAttribute(Qt::WA_InputMethodEnabled);

    // Ensure cursor is visible and blinking when the widget gets focus.
    // Qt's QPlainTextEdit has a showCursorOnInitialShow mechanism in
    // showEvent. We simplify: set showCursorOnInitialShow and handle
    // it in the first focusInEvent.
    showCursorOnInitialShow = 1;
    q->setInputMethodHints(Qt::ImhMultiLine);

#ifndef QT_NO_CURSOR
    q->viewport()->setCursor(Qt::IBeamCursor);
#endif
}

void Editor::Private::cursorPositionChanged()
{
    pageUpDownLastCursorYIsValid = false;

    // Track active atomic block and update highlighter cursor
    if (mode == Editor::Mode::LivePreview) {
        int cursorBlockNum = control->textCursor().block().blockNumber();

        // Update highlighter so it knows which blocks to show raw
        highlighter->setCursorPosition(cursorBlockNum, control->textCursor().positionInBlock());


    }
}

void Editor::Private::modificationChanged(bool)
{
}

void Editor::Private::reparseDocument()
{
    // Revert tables to pipe text before reparsing — the parser needs
    // to see raw markdown, not QTextTable objects.
    revertTables();

    // Parse for rendering (reading view, canvas cards)
    parsedDoc = Document::fromMarkdown(q->toPlainText());

    // Parse the exact editor text with tree-sitter for the highlighter.
    // Tree-sitter produces a CST with explicit delimiter nodes and byte
    // offsets that match the QTextDocument positions exactly.
    tsParser.parse(q->toPlainText());
    auto spans = tsParser.buildSpanMap();
    highlighter->setSpanMap(std::move(spans));

    detectDecoratedRanges();
    highlighter->setDecoratedRanges(decoratedRanges);
    applyBlockFormats();
    // NOTE: convertTables() is NOT called here. It must be the very last
    // step, after rehighlight() and updateBlockDisplayModes() complete,
    // because those functions iterate all blocks and would corrupt
    // QTextTable cell structure. Callers are responsible for calling
    // convertTables() at the end of their reparse sequence.
}

void Editor::Private::applyBlockFormats()
{
    if (mode != Editor::Mode::LivePreview)
        return;

    // Walk the span map to determine per-QTextBlock formatting (margins,
    // indent). QSyntaxHighlighter can only set character formats; block-level
    // formatting (left margin for blockquotes, etc.) must be set here.
    const auto &spans = highlighter->spans();
    int cursorBlockNum = control->textCursor().block().blockNumber();

    // Batch all block format changes to prevent cascading signals
    QTextCursor batchCursor(q->document());
    batchCursor.beginEditBlock();

    QTextBlock block = q->document()->begin();
    while (block.isValid()) {
        int blockStart = block.position();
        int blockEnd = blockStart + block.length();
        int blockNum = block.blockNumber();
        bool isCursorLine = (blockNum == cursorBlockNum);

        int maxBqDepth = 0;
        for (const SourceSpan &s : spans) {
            int spanEnd = s.charOffset + s.charLength;
            if (spanEnd <= blockStart) continue;
            if (s.charOffset >= blockEnd) break;
            if (s.blockquoteDepth > maxBqDepth)
                maxBqDepth = s.blockquoteDepth;
        }

        QTextBlockFormat fmt = block.blockFormat();
        // Indent by exactly the width of "> " per nesting level, so when
        // chevrons are hidden the text stays in the same position.
        QFontMetricsF fm(q->font());
        qreal chevronWidth = qCeil(fm.horizontalAdvance(QStringLiteral("> ")));
        qreal targetMargin = isCursorLine ? 0.0 : maxBqDepth * chevronWidth;

        if (fmt.leftMargin() != targetMargin) {
            batchCursor.setPosition(block.position());
            fmt.setLeftMargin(targetMargin);
            batchCursor.setBlockFormat(fmt);
        }

        block = block.next();
    }

    batchCursor.endEditBlock();
}

void Editor::Private::convertTables()
{
    if (mode != Editor::Mode::LivePreview)
        return;

    // Don't reconvert if tables already exist
    if (!liveTables.isEmpty())
        return;

    QList<ParsedTable> tables = TableHandler::detectTables(q->document());

    // Convert in reverse order so block numbers stay valid
    for (int i = tables.size() - 1; i >= 0; --i) {
        const ParsedTable &pt = tables[i];
        QTextTable *tt = TableHandler::convertToQTextTable(q->document(), pt);
        if (tt) {
            liveTables.prepend(tt);
            tableAlignments.prepend(pt.alignments);
        }
    }
}

void Editor::Private::revertTables()
{
    if (liveTables.isEmpty())
        return;


    // Revert in reverse order to preserve document positions
    for (int i = liveTables.size() - 1; i >= 0; --i) {
        QTextTable *tt = liveTables[i];
        if (!tt)
            continue;

        const auto &aligns = i < tableAlignments.size()
            ? tableAlignments[i] : QList<Qt::Alignment>();
        QString md = TableHandler::serializeToMarkdown(tt, aligns);

        // To remove a QTextTable frame entirely (not just empty its cells),
        // we must select from OUTSIDE the frame boundaries.
        // QTextFrame::firstPosition() / lastPosition() are INSIDE the frame.
        // The frame boundary characters are at firstPosition()-1 and lastPosition()+1.
        int frameStart = tt->firstPosition() - 1;
        int frameEnd = tt->lastPosition() + 1;

        QTextCursor cursor(q->document());
        cursor.setPosition(frameStart);
        cursor.setPosition(frameEnd, QTextCursor::KeepAnchor);
        cursor.beginEditBlock();
        cursor.insertText(md);  // replaceSelectedText: removes frame + inserts pipe text
        cursor.endEditBlock();
    }
    liveTables.clear();
    tableAlignments.clear();
}

void Editor::Private::checkTableCreationTrigger()
{
    if (mode != Editor::Mode::LivePreview)
        return;

    QTextCursor tc = control->textCursor();
    QTextBlock currentBlock = tc.block();
    QString currentText = currentBlock.text().trimmed();

    // Is this line a valid separator? (|---|---|)
    static const QRegularExpression separatorRe(
        QStringLiteral(R"(^\s*\|[\s:]*-+[\s:]*(\|[\s:]*-+[\s:]*)*\|\s*$)"));
    if (!separatorRe.match(currentText).hasMatch())
        return;

    // Is the previous line a valid header row? (| A | B |)
    QTextBlock prevBlock = currentBlock.previous();
    if (!prevBlock.isValid())
        return;
    static const QRegularExpression pipeRowRe(
        QStringLiteral(R"(^\s*\|.*\|\s*$)"));
    if (!pipeRowRe.match(prevBlock.text()).hasMatch())
        return;

    // We have header + separator — convert to table
    ParsedTable pt;
    pt.firstBlock = prevBlock.blockNumber();
    pt.lastBlock = currentBlock.blockNumber();
    pt.headers = TableHandler::parseRow(prevBlock.text());

    QStringList sepCells = TableHandler::parseRow(currentBlock.text());
    for (const QString &cell : sepCells)
        pt.alignments.append(TableHandler::parseAlignment(cell));
    while (pt.alignments.size() < pt.headers.size())
        pt.alignments.append(Qt::AlignLeft);

    QTextTable *tt = TableHandler::convertToQTextTable(q->document(), pt);
    if (tt) {
        liveTables.append(tt);
        tableAlignments.append(pt.alignments);

        // Place cursor in first cell of data row (row 1, col 0)
        QTextTableCell dataCell = tt->cellAt(1, 0);
        control->setTextCursor(dataCell.firstCursorPosition());

        // Cancel the pending deferred reparse — we already converted the table
        // and don't want revertTables() to undo our work and lose cursor position.
        needsReparse = false;
    }
}

void Editor::Private::updateBlockDisplayModes()
{
    if (mode != Editor::Mode::LivePreview) return;

    QTextBlock cursorBlock = control->textCursor().block();
    int cursorBlockNum = cursorBlock.blockNumber();

    QTextBlock block = q->document()->begin();
    bool anyChanged = false;

    while (block.isValid()) {
        auto *data = dynamic_cast<MarkoffBlockData *>(block.userData());
        if (!data) {
            data = new MarkoffBlockData;
            block.setUserData(data);  // QTextBlock takes ownership
        }

        int blockNum = block.blockNumber();

        bool nearCursor = (blockNum == cursorBlockNum);
        MarkoffBlockData::DisplayMode newMode = nearCursor
            ? MarkoffBlockData::Raw : MarkoffBlockData::Rendered;
        if (data->displayMode != newMode) {
            data->displayMode = newMode;
            data->cacheValid = false;  // invalidate cache on mode change
            anyChanged = true;
        }

        block = block.next();
    }

    if (anyChanged)
        q->viewport()->update();
}

void Editor::Private::renderBlock(QTextBlock &block)
{
    auto *data = dynamic_cast<MarkoffBlockData *>(block.userData());
    if (!data || data->cacheValid) return;

    // Get the block's text
    QString blockText = block.text();
    if (blockText.trimmed().isEmpty()) {
        data->renderedHeight = block.layout()->boundingRect().height();
        data->cacheValid = true;
        data->renderedCache = QPixmap();  // empty -- just use normal paint
        return;
    }

    // Render this block's markdown through our renderer
    // Rendered blocks use RenderSettings (same as reading view) — not the
    // editor's source font. The layout engine handles the height difference.
    auto blockDoc = Document::fromMarkdown(blockText);
    auto rendered = renderer.renderToTextDocument(*blockDoc);

    // Match the editor's document margin so rendered content aligns
    // with raw text at the same x-offset
    rendered->setDocumentMargin(q->document()->documentMargin());

    // Strip paragraph margins from rendered blocks — the editor's layout
    // already handles inter-block spacing. Without this, we get double
    // spacing (editor block gap + rendered <p> margins).
    QTextBlock renderedBlock = rendered->begin();
    while (renderedBlock.isValid()) {
        QTextCursor c(renderedBlock);
        QTextBlockFormat fmt = renderedBlock.blockFormat();
        fmt.setTopMargin(0);
        fmt.setBottomMargin(0);
        c.setBlockFormat(fmt);
        renderedBlock = renderedBlock.next();
    }

    // Get the viewport width for rendering
    int width = q->viewport()->width();
    rendered->setTextWidth(width);

    int height = qMax(static_cast<int>(rendered->size().height()),
                      static_cast<int>(block.layout()->boundingRect().height()));

    if (width > 0 && height > 0) {
        QPixmap pixmap(width, height);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        rendered->drawContents(&painter);
        painter.end();

        data->renderedCache = pixmap;
        data->renderedHeight = height;
    }
    data->cacheValid = true;
}

// ---------------------------------------------------------------------------
// Atomic block management
// ---------------------------------------------------------------------------

void Editor::Private::detectDecoratedRanges()
{
    decoratedRanges.clear();

    if (mode != Editor::Mode::LivePreview)
        return;

    QTextDocument *doc = control->document();
    QTextBlock block = doc->begin();

    // Detect fenced code blocks: ``` ... ```
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
                decoratedRanges.append(dr);
            }
        }
        if (block.isValid())
            block = block.next();
    }

    // Detect callout blocks (> [!type] ...)
    block = doc->begin();
    static const QRegularExpression calloutRe(
        QStringLiteral(R"(^>\s*\[!(\w+)\]([+-])?\s*(.*)?$)"));

    while (block.isValid()) {
        // Skip blocks in code block ranges
        bool inCodeBlock = false;
        for (const auto &dr : decoratedRanges) {
            if (dr.type == DecoratedRange::CodeBlock &&
                block.blockNumber() >= dr.firstBlock && block.blockNumber() <= dr.lastBlock) {
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
            decoratedRanges.append(dr);

            block = bodyBlock; // skip past the callout
            continue;
        }

        block = block.next();
    }

    // Detect tables and add as decorated ranges
    QList<ParsedTable> tables = TableHandler::detectTables(q->document());
    for (const ParsedTable &pt : tables) {
        DecoratedRange dr;
        dr.type = DecoratedRange::Table;
        dr.firstBlock = pt.firstBlock;
        dr.lastBlock = pt.lastBlock;
        decoratedRanges.append(dr);
    }
}

const DecoratedRange *Editor::Private::decoratedRangeAt(int blockNumber) const
{
    for (const auto &dr : decoratedRanges) {
        if (blockNumber >= dr.firstBlock && blockNumber <= dr.lastBlock)
            return &dr;
    }
    return nullptr;
}

void Editor::Private::verticalScrollbarActionTriggered(int action)
{
    const auto a = static_cast<QAbstractSlider::SliderAction>(action);
    switch (a) {
    case QAbstractSlider::SliderPageStepAdd:
        pageUpDown(QTextCursor::Down, QTextCursor::MoveAnchor, false);
        break;
    case QAbstractSlider::SliderPageStepSub:
        pageUpDown(QTextCursor::Up, QTextCursor::MoveAnchor, false);
        break;
    default:
        break;
    }
}

qreal Editor::Private::verticalOffset() const
{
    // Pixel-based scrolling: scrollbar value IS the pixel offset.
    // Subtract document margin so the first block starts at the margin.
    return q->verticalScrollBar()->value() - q->document()->documentMargin();
}

qreal Editor::Private::blockPixelPosition(const QTextBlock &block) const
{
    // Compute the cumulative Y position of a block in the document.
    PlainTextDocumentLayout *dl = qobject_cast<PlainTextDocumentLayout*>(
        control->document()->documentLayout());
    Q_ASSERT(dl);
    qreal y = 0;
    QTextBlock b = control->document()->begin();
    while (b.isValid() && b != block) {
        y += dl->blockBoundingRect(b).height();
        b = b.next();
    }
    return y;
}

void Editor::Private::setTopLine(int /*visualTopLine*/, int /*dx*/)
{
    // Legacy line-based entry point — no longer used with pixel scrolling.
    // scrollContentsBy() drives scrolling directly via the scrollbar value.
}

void Editor::Private::setTopBlock(int blockNumber, int /*lineNumber*/, int /*dx*/)
{
    // Scroll so that the given block is at the top of the viewport.
    qreal y = blockPixelPosition(q->document()->findBlockByNumber(qMax(0, blockNumber)));
    q->verticalScrollBar()->setValue(static_cast<int>(y + q->document()->documentMargin()));
}

void Editor::Private::ensureVisible(int position, bool center, bool forceCenter) {
    QTextBlock block = control->document()->findBlock(position);
    if (!block.isValid())
        return;

    // Compute the pixel Y of the cursor position
    qreal blockY = blockPixelPosition(block) + q->document()->documentMargin();
    QTextLine line = block.layout()->lineForTextPosition(position - block.position());
    qreal cursorY = blockY;
    if (line.isValid())
        cursorY += line.naturalTextRect().top();

    auto *vbar = q->verticalScrollBar();
    int scrollY = vbar->value();
    int vpHeight = q->viewport()->height();

    if (forceCenter || center) {
        vbar->setValue(static_cast<int>(cursorY - vpHeight / 2));
    } else if (cursorY < scrollY) {
        // Cursor above viewport — scroll up
        vbar->setValue(static_cast<int>(cursorY));
    } else if (cursorY + (line.isValid() ? line.height() : 20) > scrollY + vpHeight) {
        // Cursor below viewport — scroll down to show it at bottom
        vbar->setValue(static_cast<int>(cursorY + (line.isValid() ? line.height() : 20) - vpHeight));
    }
}

void Editor::Private::updateViewport()
{
    q->viewport()->update();
}

void Editor::Private::repaintContents(const QRectF &contentsRect)
{
    if (!contentsRect.isValid()) {
        updateViewport();
        return;
    }
    const int xOffset = horizontalOffset();
    const int yOffset = (int)verticalOffset();
    const QRect visibleRect(xOffset, yOffset, q->viewport()->width(), q->viewport()->height());

    QRect r = contentsRect.adjusted(-1, -1, 1, 1).intersected(visibleRect).toAlignedRect();
    if (r.isEmpty())
        return;

    r.translate(-xOffset, -yOffset);
    q->viewport()->update(r);
}

void Editor::Private::pageUpDown(QTextCursor::MoveOperation op, QTextCursor::MoveMode moveMode, bool moveCursor)
{
    auto *vbar = q->verticalScrollBar();
    auto *vp = q->viewport();

    QTextCursor cursor = control->textCursor();
    if (moveCursor) {
        ensureCursorVisible();
        if (!pageUpDownLastCursorYIsValid)
            pageUpDownLastCursorY = control->cursorRect(cursor).top() - verticalOffset();
    }

    qreal lastY = pageUpDownLastCursorY;

    if (op == QTextCursor::Down) {
        QRectF visible = QRectF(vp->rect()).translated(
            -(q->isRightToLeft() ? (q->horizontalScrollBar()->maximum() - q->horizontalScrollBar()->value()) : q->horizontalScrollBar()->value()),
            -verticalOffset());
        QTextBlock firstVB = control->firstVisibleBlock();
        QTextBlock block = firstVB;

        PlainTextDocumentLayout *docLayout = qobject_cast<PlainTextDocumentLayout*>(
            control->document()->documentLayout());
        Q_ASSERT(docLayout);

        QRectF br = docLayout->blockBoundingRect(block);
        qreal h = 0;
        int atEnd = false;
        while (h + br.height() <= visible.bottom()) {
            if (!block.next().isValid()) {
                atEnd = true;
                lastY = visible.bottom();
                break;
            }
            h += br.height();
            block = block.next();
            br = docLayout->blockBoundingRect(block);
        }

        if (!atEnd) {
            int line = 0;
            qreal diff = visible.bottom() - h;
            int lineCount = block.layout()->lineCount();
            while (line < lineCount - 1) {
                if (block.layout()->lineAt(line).naturalTextRect().bottom() > diff)
                    break;
                ++line;
            }
            setTopBlock(block.blockNumber(), line);
        }

        if (moveCursor) {
            lastY += verticalOffset();
            bool moved = false;
            do {
                moved = cursor.movePosition(op, moveMode);
            } while (moved && control->cursorRect(cursor).top() < lastY);
        }

    } else if (op == QTextCursor::Up) {
        QRectF visible = QRectF(vp->rect()).translated(
            -(q->isRightToLeft() ? (q->horizontalScrollBar()->maximum() - q->horizontalScrollBar()->value()) : q->horizontalScrollBar()->value()),
            -verticalOffset());
        visible.translate(0, -visible.height());
        QTextBlock block = control->firstVisibleBlock();

        PlainTextDocumentLayout *docLayout = qobject_cast<PlainTextDocumentLayout*>(
            control->document()->documentLayout());
        Q_ASSERT(docLayout);

        qreal h = 0;
        while (h >= visible.top()) {
            if (!block.previous().isValid()) {
                if (q->verticalScrollBar()->value() == 0) {
                    lastY = 0;
                }
                break;
            }
            block = block.previous();
            QRectF br = docLayout->blockBoundingRect(block);
            h -= br.height();
        }

        int line = 0;
        if (block.isValid()) {
            qreal diff = visible.top() - h;
            int lineCount = block.layout()->lineCount();
            while (line < lineCount) {
                if (block.layout()->lineAt(line).naturalTextRect().top() >= diff)
                    break;
                ++line;
            }
            if (line == lineCount) {
                if (block.next().isValid() && block.next() != control->firstVisibleBlock()) {
                    block = block.next();
                    line = 0;
                } else {
                    --line;
                }
            }
        }
        setTopBlock(block.blockNumber(), line);

        if (moveCursor) {
            cursor.setVisualNavigation(true);
            lastY += verticalOffset();
            bool moved = false;
            do {
                moved = cursor.movePosition(op, moveMode);
            } while (moved && control->cursorRect(cursor).top() > lastY);
        }
    }

    if (moveCursor) {
        control->setTextCursor(cursor, moveMode == QTextCursor::KeepAnchor);
        pageUpDownLastCursorYIsValid = true;
    }
}

void Editor::Private::adjustScrollbars()
{
    auto *vbar = q->verticalScrollBar();
    auto *hbar = q->horizontalScrollBar();
    auto *vp = q->viewport();

    QTextDocument *doc = control->document();
    PlainTextDocumentLayout *documentLayout = qobject_cast<PlainTextDocumentLayout*>(doc->documentLayout());
    Q_ASSERT(documentLayout);
    bool documentSizeChangedBlocked = documentLayout->priv()->blockDocumentSizeChanged;
    documentLayout->priv()->blockDocumentSizeChanged = true;
    qreal margin = doc->documentMargin();

    // Pixel-based scrolling: compute total document height in pixels.
    qreal totalHeight = margin;
    QTextBlock block = doc->begin();
    while (block.isValid()) {
        totalHeight += documentLayout->blockBoundingRect(block).height();
        block = block.next();
    }
    totalHeight += margin;

    int viewportHeight = vp->height();
    int vmax = qMax(0, static_cast<int>(totalHeight - viewportHeight));

    vbar->setRange(0, vmax);
    vbar->setPageStep(viewportHeight);
    vbar->setSingleStep(q->fontMetrics().lineSpacing());

    QSizeF documentSize = documentLayout->documentSize();
    hbar->setRange(0, (int)documentSize.width() - vp->width());
    hbar->setPageStep(vp->width());
    documentLayout->priv()->blockDocumentSizeChanged = documentSizeChangedBlocked;
}

void Editor::Private::ensureViewportLayouted()
{
}

void Editor::Private::relayoutDocument()
{
    QTextDocument *doc = control->document();
    PlainTextDocumentLayout *documentLayout = qobject_cast<PlainTextDocumentLayout*>(doc->documentLayout());
    Q_ASSERT(documentLayout);
    documentLayoutPtr = documentLayout;

    int width = q->viewport()->width();

    if (documentLayout->priv()->mainViewPrivate == nullptr
        || documentLayout->priv()->mainViewPrivate == this
        || width > documentLayout->textWidth()) {
        documentLayout->priv()->mainViewPrivate = this;
        documentLayout->setTextWidth(width);
    }
}

void Editor::Private::updateDefaultTextOption()
{
    QTextDocument *doc = control->document();

    QTextOption opt = doc->defaultTextOption();
    QTextOption::WrapMode oldWrapMode = opt.wrapMode();

    // Always wrap at widget width (default for Markoff)
    opt.setWrapMode(wordWrap);

    if (opt.wrapMode() != oldWrapMode)
        doc->setDefaultTextOption(opt);
}

void Editor::Private::ensureCursorVisible(bool center)
{
    QRect visible = q->viewport()->rect();
    QRect cr = q->cursorRect();
    if (cr.top() < visible.top() || cr.bottom() > visible.bottom()) {
        ensureVisible(control->textCursor().position(), center);
    }

    const bool rtl = q->isRightToLeft();
    if (cr.left() < visible.left() || cr.right() > visible.right()) {
        auto *hbar = q->horizontalScrollBar();
        int x = cr.center().x() + horizontalOffset() - visible.width()/2;
        hbar->setValue(rtl ? hbar->maximum() - x : x);
    }
}


// ============================================================================
// Editor (public widget)
// ============================================================================

static void fillBackground(QPainter *p, const QRectF &rect, QBrush brush, const QRectF &gradientRect = QRectF())
{
    p->save();
    if (brush.style() >= Qt::LinearGradientPattern && brush.style() <= Qt::ConicalGradientPattern) {
        if (!gradientRect.isNull()) {
            QTransform m = QTransform::fromTranslate(gradientRect.left(), gradientRect.top());
            m.scale(gradientRect.width(), gradientRect.height());
            brush.setTransform(m);
            const_cast<QGradient *>(brush.gradient())->setCoordinateMode(QGradient::LogicalMode);
        }
    } else {
        p->setBrushOrigin(rect.topLeft());
    }
    p->fillRect(rect, brush);
    p->restore();
}

Editor::Editor(QWidget *parent)
    : QAbstractScrollArea(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->init();
}

Editor::~Editor()
{
    if (d->documentLayoutPtr) {
        if (d->documentLayoutPtr->priv()->mainViewPrivate == d.get())
            d->documentLayoutPtr->priv()->mainViewPrivate = nullptr;
    }
}

void Editor::setPlainText(const QString &text)
{
    d->control->setPlainText(text);
}

QString Editor::toPlainText() const
{
    return document()->toPlainText();
}

QTextDocument *Editor::document() const
{
    return d->control->document();
}

void Editor::setMode(Mode m)
{
    if (d->mode == m) return;
    d->mode = m;

    // Default source mode margin (QTextDocument default)
    static constexpr qreal sourceMargin = 4.0;
    // Live preview margin — matches the renderer's typographic margin
    // so raw and rendered lines align at the same x-offset
    static constexpr qreal livePreviewMargin = 20.0;

    if (m == Mode::LivePreview) {
        document()->setDocumentMargin(livePreviewMargin);

        // Update the renderer to use zero internal margin (the document
        // margin handles it) so rendered blocks align with raw text
        RenderSettings settings = d->renderer.settings();
        settings.marginPx = 0;
        d->renderer.setSettings(settings);

        // Parse first so spans are available, THEN switch mode
        // (setMode calls rehighlight, which needs the spans)
        d->reparseDocument();
        d->highlighter->setCursorPosition(d->control->textCursor().block().blockNumber(),
                                          d->control->textCursor().positionInBlock());
        d->highlighter->setMode(MarkdownHighlighter::Mode::LivePreview);
        d->updateBlockDisplayModes();
        // Convert tables LAST, after all block iteration is complete
        d->convertTables();
    } else {
        // Revert QTextTables to pipe markdown before switching to source
        d->revertTables();

        document()->setDocumentMargin(sourceMargin);
        d->highlighter->setMode(MarkdownHighlighter::Mode::Source);

        // Source mode: clear all rendered caches, set all blocks to Raw
        QTextBlock block = document()->begin();
        while (block.isValid()) {
            auto *data = dynamic_cast<MarkoffBlockData *>(block.userData());
            if (data) {
                data->displayMode = MarkoffBlockData::Raw;
                data->cacheValid = false;
            }
            block = block.next();
        }
    }
    d->adjustScrollbars();
    viewport()->update();
}

Editor::Mode Editor::mode() const
{
    return d->mode;
}

void Editor::setFontSize(int pointSize)
{
    QFont f = font();
    f.setPointSize(pointSize);
    setFont(f);
    document()->setDefaultFont(f);

    QTextBlock block = document()->begin();
    while (block.isValid()) {
        auto *data = dynamic_cast<MarkoffBlockData *>(block.userData());
        if (data)
            data->cacheValid = false;
        block = block.next();
    }
    viewport()->update();
}

void Editor::ensureCursorVisible()
{
    d->ensureCursorVisible(d->centerOnScroll);
}

QPointF contentOffset(const Editor::Private *d, const Editor *q)
{
    // Pixel-based scrolling: the Y offset positions the first visible block
    // correctly in the viewport. We compute the cumulative pixel position of
    // the first visible block and subtract the scroll position.
    QTextBlock fvb = d->control->firstVisibleBlock();
    qreal blockY = d->blockPixelPosition(fvb) + q->document()->documentMargin();
    qreal scrollY = q->verticalScrollBar()->value();
    return QPointF(-d->horizontalOffset(), blockY - scrollY);
}

QRect Editor::cursorRect() const
{
    QRect r = d->control->cursorRect().toRect();
    r.translate(-d->horizontalOffset(), -(int)d->verticalOffset());
    return r;
}

QTextBlock firstVisibleBlock(const Editor::Private *d)
{
    return d->control->firstVisibleBlock();
}

QRectF blockBoundingRect(const Editor::Private *d, const QTextBlock &block)
{
    PlainTextDocumentLayout *documentLayout = qobject_cast<PlainTextDocumentLayout*>(
        d->control->document()->documentLayout());
    Q_ASSERT(documentLayout);
    return documentLayout->blockBoundingRect(block);
}

void Editor::paintTable(QPainter *painter, QTextTable *table,
                        const QRectF &tableRect, const QRect &viewportRect)
{
    auto *td = static_cast<TableLayoutData *>(table->layoutData());
    if (!td || td->dirty) return;

    const int rows = table->rows();
    const int cols = table->columns();
    const qreal margin = document()->documentMargin();

    // The table rect's top-left gives us the position in viewport coords.
    // td->columnPositions and td->rowPositions are relative to the table origin.
    const qreal tableX = tableRect.left() + margin;
    const qreal tableY = tableRect.top();

    painter->save();

    // Background
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0xfa, 0xfa, 0xfa));
    painter->drawRect(QRectF(tableX, tableY, td->tableWidth, td->tableHeight));

    // Grid lines
    painter->setPen(QPen(QColor(0xe0, 0xe0, 0xe0), 1));

    // Horizontal lines
    for (int r = 0; r <= rows; ++r) {
        qreal y;
        if (r < rows)
            y = tableY + td->rowPositions[r];
        else
            y = tableY + td->rowPositions[rows - 1] + td->heights[rows - 1] + td->cellSpacing;
        painter->drawLine(QPointF(tableX, y),
                          QPointF(tableX + td->tableWidth, y));
    }

    // Vertical lines
    for (int c = 0; c <= cols; ++c) {
        qreal x;
        if (c < cols)
            x = tableX + td->columnPositions[c];
        else
            x = tableX + td->columnPositions[cols - 1] + td->widths[cols - 1] + td->cellSpacing;
        painter->drawLine(QPointF(x, tableY),
                          QPointF(x, tableY + td->tableHeight));
    }

    // Thicker header separator
    if (rows > 1) {
        painter->setPen(QPen(QColor(0xc0, 0xc0, 0xc0), 2));
        qreal sepY = tableY + td->rowPositions[1];
        painter->drawLine(QPointF(tableX, sepY),
                          QPointF(tableX + td->tableWidth, sepY));
    }

    // Cell text
    QFont cellFont = font();
    QFontMetricsF fm(cellFont);
    for (int r = 0; r < rows; ++r) {
        if (r == 0) {
            QFont bold = cellFont;
            bold.setWeight(QFont::Bold);
            painter->setFont(bold);
        } else if (r == 1) {
            painter->setFont(cellFont);
        }
        painter->setPen(palette().color(QPalette::Text));

        for (int c = 0; c < cols; ++c) {
            QTextTableCell cell = table->cellAt(r, c);
            if (cell.row() != r || cell.column() != c) continue;  // skip spanned

            // Get cell text (may span multiple blocks)
            QString text;
            QTextBlock b = cell.firstCursorPosition().block();
            QTextBlock lastB = cell.lastCursorPosition().block();
            while (b.isValid()) {
                if (!text.isEmpty()) text += QLatin1Char(' ');
                text += b.text();
                if (b == lastB) break;
                b = b.next();
            }

            // Cell rect in viewport coordinates
            qreal cellX = tableX + td->columnPositions[c] + td->cellPadding;
            qreal cellY = tableY + td->rowPositions[r] + td->cellPadding;
            qreal cellW = td->widths[c] - td->cellPadding * 2;
            qreal cellH = td->heights[r] - td->cellPadding * 2;

            QRectF textRect(cellX, cellY, cellW, cellH);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                              fm.elidedText(text, Qt::ElideRight, cellW));
        }
    }

    // Cursor highlight: if cursor is inside this table, highlight the cell
    QTextCursor tc = d->control->textCursor();
    QTextTable *cursorTable = tc.currentTable();
    if (cursorTable == table) {
        QTextTableCell curCell = table->cellAt(tc);
        if (curCell.isValid()) {
            int cr = curCell.row();
            int cc = curCell.column();
            QRectF highlight(tableX + td->columnPositions[cc],
                             tableY + td->rowPositions[cr],
                             td->widths[cc],
                             td->heights[cr]);
            painter->setPen(QPen(palette().color(QPalette::Highlight), 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(highlight.adjusted(1, 1, -1, -1));
        }
    }

    // Hover chrome: + buttons at edges when hovering over a table
    if (d->hoverTable == table) {
        QFont smallFont = font();
        smallFont.setPointSize(qMax(8, font().pointSize() - 2));
        painter->setFont(smallFont);

        // + button at right edge (append column)
        {
            qreal x = tableX + td->tableWidth + 4;
            qreal y = tableY + td->tableHeight / 2 - 10;
            QRectF btnRect(x, y, 20, 20);
            painter->setPen(QPen(QColor(0x99, 0x99, 0x99), 1));
            painter->setBrush(QColor(0xf0, 0xf0, 0xf0));
            painter->drawRoundedRect(btnRect, 3, 3);
            painter->setPen(QColor(0x66, 0x66, 0x66));
            painter->drawText(btnRect, Qt::AlignCenter, QStringLiteral("+"));
        }

        // + button at bottom edge (append row)
        {
            qreal x = tableX + td->tableWidth / 2 - 10;
            qreal y = tableY + td->tableHeight + 4;
            QRectF btnRect(x, y, 20, 20);
            painter->setPen(QPen(QColor(0x99, 0x99, 0x99), 1));
            painter->setBrush(QColor(0xf0, 0xf0, 0xf0));
            painter->drawRoundedRect(btnRect, 3, 3);
            painter->setPen(QColor(0x66, 0x66, 0x66));
            painter->drawText(btnRect, Qt::AlignCenter, QStringLiteral("+"));
        }
    }

    painter->restore();
}

void Editor::paintEvent(QPaintEvent *e)
{
    QPainter painter(viewport());
    Q_ASSERT(qobject_cast<PlainTextDocumentLayout*>(document()->documentLayout()));

    QPointF offset = Markoff::contentOffset(d.get(), this);

    QRect er = e->rect();
    QRect viewportRect = viewport()->rect();

    bool editable = true; // always editable in Markoff

    QTextBlock block = Markoff::firstVisibleBlock(d.get());
    qreal maximumWidth = document()->documentLayout()->documentSize().width();

    painter.setBrushOrigin(offset);

    int maxX = offset.x() + qMax((qreal)viewportRect.width(), maximumWidth)
               - document()->documentMargin() + d->control->cursorWidth();
    er.setRight(qMin(er.right(), maxX));
    painter.setClipRect(er);

    // Get paint context
    QAbstractTextDocumentLayout::PaintContext context = d->control->getPaintContext(viewport());
    painter.setPen(context.palette.text().color());

    while (block.isValid()) {
        QRectF r = Markoff::blockBoundingRect(d.get(), block).translated(offset);
        QTextLayout *layout = block.layout();

        if (!block.isVisible()) {
            offset.ry() += r.height();
            block = block.next();
            continue;
        }

        if (r.bottom() >= er.top() && r.top() <= er.bottom()) {

            // Table blocks: paint the entire table, then skip past it
            QTextTable *table = tableForBlock(block);
            if (table && isFirstTableBlock(block, table)) {
                paintTable(&painter, table, r, viewportRect);
                // Skip to the block after the table
                QTextTableCell lastCell = table->cellAt(table->rows() - 1,
                                                         table->columns() - 1);
                block = lastCell.lastCursorPosition().block();
                offset.ry() += r.height();
                block = block.next();
                continue;
            }

            // Skip non-first table blocks (their height is 0)
            if (table) {
                block = block.next();
                continue;
            }

            // Live preview: paint decorations BEHIND text for decorated ranges.
            // The text draws normally afterward via layout->draw().
            if (d->mode == Mode::LivePreview) {
                const DecoratedRange *dr = d->decoratedRangeAt(block.blockNumber());
                if (dr && block.blockNumber() == dr->firstBlock) {
                    // First block of a decorated range: paint the background
                    // spanning all blocks in the range.
                    qreal margin = document()->documentMargin();
                    qreal rangeHeight = 0;
                    QTextBlock b = document()->findBlockByNumber(dr->firstBlock);
                    for (int i = dr->firstBlock; i <= dr->lastBlock && b.isValid(); ++i, b = b.next())
                        rangeHeight += Markoff::blockBoundingRect(d.get(), b).height();

                    QRectF bgRect(r.left() + margin - 4, r.top(),
                                  viewportRect.width() - margin * 2 + 8, rangeHeight);

                    if (dr->type == DecoratedRange::CodeBlock) {
                        // Gray background with rounded corners
                        painter.save();
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(QColor(0xf5, 0xf5, 0xf5));
                        painter.setRenderHint(QPainter::Antialiasing);
                        painter.drawRoundedRect(bgRect, 4, 4);
                        // Border
                        painter.setPen(QPen(QColor(0xe0, 0xe0, 0xe0), 1));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
                        // Language label
                        if (!dr->language.isEmpty()) {
                            QFont labelFont = font();
                            labelFont.setPointSize(qMax(8, font().pointSize() - 2));
                            painter.setFont(labelFont);
                            painter.setPen(QColor(0x9e, 0x9e, 0x9e));
                            QRectF labelRect(bgRect.right() - 80, bgRect.top() + 2, 72, 16);
                            painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, dr->language);
                        }
                        painter.restore();
                    } else if (dr->type == DecoratedRange::Callout) {
                        // Colored left border + faint background
                        painter.save();
                        painter.setRenderHint(QPainter::Antialiasing);
                        QColor bg = dr->calloutColor;
                        bg.setAlpha(20);
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(bg);
                        painter.drawRoundedRect(bgRect, 4, 4);
                        // Left border
                        painter.setBrush(dr->calloutColor);
                        painter.drawRoundedRect(QRectF(bgRect.left(), bgRect.top(), 4, bgRect.height()), 2, 2);

                        // Title styling handled by the highlighter (hideRange +
                        // colored text), same approach as bold/italic.
                        // TODO: for no-title callouts, paint default title here.
                        painter.restore();
                    }
                }

                // Paint blockquote vertical lines in the indent space.
                // One line per nesting level, dark gray.
                qreal leftMargin = block.blockFormat().leftMargin();
                if (leftMargin > 0) {
                    const auto &spans = d->highlighter->spans();
                    int bqDepth = 0;
                    int blockStart = block.position();
                    int blockEnd = blockStart + block.length();
                    for (const SourceSpan &s : spans) {
                        int se = s.charOffset + s.charLength;
                        if (se <= blockStart) continue;
                        if (s.charOffset >= blockEnd) break;
                        if (s.blockquoteDepth > bqDepth)
                            bqDepth = s.blockquoteDepth;
                    }

                    if (bqDepth > 0) {
                        // Check if this block is inside a callout (callouts paint their own border)
                        const DecoratedRange *calloutDr = d->decoratedRangeAt(block.blockNumber());
                        bool isCallout = calloutDr && calloutDr->type == DecoratedRange::Callout;

                        if (!isCallout) {
                            painter.save();
                            qreal margin = document()->documentMargin();
                            QFontMetricsF fm(font());
                            qreal chevronWidth = qCeil(fm.horizontalAdvance(QStringLiteral("> ")));
                            // Derive line color from blockquote text color, lighter
                            QColor bqTextColor = d->highlighter->blockquoteColor();
                            QColor lineColor = bqTextColor.lighter(140);
                            painter.setPen(QPen(lineColor, 2));

                            for (int level = 0; level < bqDepth; ++level) {
                                qreal x = offset.x() + margin + level * chevronWidth + 1;
                                painter.drawLine(QPointF(x, r.top()),
                                                QPointF(x, r.top() + r.height()));
                            }
                            painter.restore();
                        }
                    }
                }

                // Paint horizontal rules — a line where --- or *** text is
                {
                    const auto &spans = d->highlighter->spans();
                    int blockStart = block.position();
                    int blockEnd = blockStart + block.length();
                    bool isHR = false;
                    for (const SourceSpan &s : spans) {
                        int se = s.charOffset + s.charLength;
                        if (se <= blockStart) continue;
                        if (s.charOffset >= blockEnd) break;
                        if (s.isHorizontalRule) { isHR = true; break; }
                    }
                    int cursorBlockNum = d->control->textCursor().block().blockNumber();
                    if (isHR && block.blockNumber() != cursorBlockNum) {
                        painter.save();
                        qreal margin = document()->documentMargin();
                        qreal y = r.top() + r.height() / 2;
                        qreal x1 = margin;
                        qreal x2 = viewportRect.width() - margin;
                        painter.setPen(QPen(QColor(0xcc, 0xcc, 0xcc), 1));
                        painter.drawLine(QPointF(x1, y), QPointF(x2, y));
                        painter.restore();
                    }
                }

                // Fall through to normal text painting
            }

            QTextBlockFormat blockFormat = block.blockFormat();

            QBrush bg = blockFormat.background();
            if (bg != Qt::NoBrush) {
                QRectF contentsRect = r;
                contentsRect.setWidth(qMax(r.width(), maximumWidth));
                fillBackground(&painter, contentsRect, bg);
            }

            QList<QTextLayout::FormatRange> selections;
            int blpos = block.position();
            int bllen = block.length();
            for (int i = 0; i < context.selections.size(); ++i) {
                const QAbstractTextDocumentLayout::Selection &range = context.selections.at(i);
                const int selStart = range.cursor.selectionStart() - blpos;
                const int selEnd = range.cursor.selectionEnd() - blpos;
                if (selStart < bllen && selEnd > 0
                    && selEnd > selStart) {
                    QTextLayout::FormatRange o;
                    o.start = selStart;
                    o.length = selEnd - selStart;
                    o.format = range.format;
                    selections.append(o);
                } else if (!range.cursor.hasSelection() && range.format.hasProperty(QTextFormat::FullWidthSelection)
                           && block.contains(range.cursor.position())) {
                    QTextLayout::FormatRange o;
                    QTextLine l = layout->lineForTextPosition(range.cursor.position() - blpos);
                    o.start = l.textStart();
                    o.length = l.textLength();
                    if (o.start + o.length == bllen - 1)
                        ++o.length;
                    o.format = range.format;
                    selections.append(o);
                }
            }

            bool drawCursor = (context.cursorPosition >= blpos
                               && context.cursorPosition < blpos + bllen);

            bool drawCursorAsBlock = drawCursor && d->control->overwriteMode();

            if (drawCursorAsBlock) {
                if (context.cursorPosition == blpos + bllen - 1) {
                    drawCursorAsBlock = false;
                } else {
                    QTextLayout::FormatRange o;
                    o.start = context.cursorPosition - blpos;
                    o.length = 1;
                    o.format.setForeground(palette().base());
                    o.format.setBackground(palette().text());
                    selections.append(o);
                }
            }

            // Apply blockquote indentation by offsetting the draw position.
            // Our PlainTextDocumentLayout ignores QTextBlockFormat::leftMargin,
            // so we apply it here in the paint path.
            QPointF drawOffset = offset;
            if (d->mode == Mode::LivePreview) {
                qreal leftMargin = block.blockFormat().leftMargin();
                if (leftMargin > 0)
                    drawOffset.rx() += leftMargin;
            }

            layout->draw(&painter, drawOffset, selections, er);

            if ((drawCursor && !drawCursorAsBlock)
                || (editable && context.cursorPosition < -1
                    && !layout->preeditAreaText().isEmpty())) {
                int cpos = context.cursorPosition;
                if (cpos < -1)
                    cpos = layout->preeditAreaPosition() - (cpos + 2);
                else
                    cpos -= blpos;
                layout->drawCursor(&painter, drawOffset, cpos, d->control->cursorWidth());
            }
        }

        offset.ry() += r.height();
        if (offset.y() > viewportRect.height())
            break;
        block = block.next();
    }

    if (d->backgroundVisible && !block.isValid() && offset.y() <= er.bottom()
        && (d->centerOnScroll || verticalScrollBar()->maximum() == verticalScrollBar()->minimum())) {
        painter.fillRect(QRect(QPoint((int)er.left(), (int)offset.y()), er.bottomRight()), palette().window());
    }
}

void Editor::resizeEvent(QResizeEvent *e)
{
    if (e->oldSize().width() != e->size().width())
        d->relayoutDocument();
    d->adjustScrollbars();
}

void Editor::keyPressEvent(QKeyEvent *e)
{
    // Atomic block input routing
            e->accept();
#ifndef QT_NO_SHORTCUT
    Qt::TextInteractionFlags tif = d->control->textInteractionFlags();

    if (tif & Qt::TextSelectableByKeyboard) {
        if (e == QKeySequence::SelectPreviousPage) {
            e->accept();
            d->pageUpDown(QTextCursor::Up, QTextCursor::KeepAnchor);
            return;
        } else if (e == QKeySequence::SelectNextPage) {
            e->accept();
            d->pageUpDown(QTextCursor::Down, QTextCursor::KeepAnchor);
            return;
        }
    }
    if (tif & (Qt::TextSelectableByKeyboard | Qt::TextEditable)) {
        if (e == QKeySequence::MoveToPreviousPage) {
            e->accept();
            d->pageUpDown(QTextCursor::Up, QTextCursor::MoveAnchor);
            return;
        } else if (e == QKeySequence::MoveToNextPage) {
            e->accept();
            d->pageUpDown(QTextCursor::Down, QTextCursor::MoveAnchor);
            return;
        }
    }

    if (!(tif & Qt::TextEditable)) {
        switch (e->key()) {
            case Qt::Key_Space:
                e->accept();
                if (e->modifiers() & Qt::ShiftModifier)
                    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
                else
                    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
                break;
            default:
                d->sendControlEvent(e);
                if (!e->isAccepted() && e->modifiers() == Qt::NoModifier) {
                    if (e->key() == Qt::Key_Home) {
                        verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
                        e->accept();
                    } else if (e->key() == Qt::Key_End) {
                        verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);
                        e->accept();
                    }
                }
                if (!e->isAccepted()) {
                    QAbstractScrollArea::keyPressEvent(e);
                }
        }
        return;
    }
#endif // QT_NO_SHORTCUT

    d->sendControlEvent(e);

    // After input: check if typing | completed a table separator line
    if (e->text() == QStringLiteral("|") && d->mode == Mode::LivePreview) {
        d->checkTableCreationTrigger();
    }
}

void Editor::mousePressEvent(QMouseEvent *e)
{
    // Check if click lands on a table + button
    if (d->mode == Mode::LivePreview && d->hoverTable && e->button() == Qt::LeftButton) {
        auto *td = static_cast<TableLayoutData *>(d->hoverTable->layoutData());
        if (td && !td->dirty) {
            QPointF pos = e->position();
            const qreal margin = document()->documentMargin();
            QPointF offset = Markoff::contentOffset(d.get(), this);
            QTextBlock firstBlock = d->hoverTable->cellAt(0, 0).firstCursorPosition().block();
            QRectF tableRect = Markoff::blockBoundingRect(d.get(), firstBlock).translated(offset);
            qreal tableX = tableRect.left() + margin;
            qreal tableY = tableRect.top();

            // Right + button (append column)
            QRectF rightBtn(tableX + td->tableWidth + 4,
                            tableY + td->tableHeight / 2 - 10, 20, 20);
            if (rightBtn.contains(pos)) {
                d->hoverTable->appendColumns(1);
                viewport()->update();
                e->accept();
                return;
            }

            // Bottom + button (append row)
            QRectF bottomBtn(tableX + td->tableWidth / 2 - 10,
                             tableY + td->tableHeight + 4, 20, 20);
            if (bottomBtn.contains(pos)) {
                d->hoverTable->appendRows(1);
                viewport()->update();
                e->accept();
                return;
            }
        }
    }

    d->mouseDragging = true;
    d->sendControlEvent(e);
}

void Editor::mouseMoveEvent(QMouseEvent *e)
{
    d->inDrag = false;
    const QPoint pos = e->position().toPoint();
    d->sendControlEvent(e);

    // Update table hover state for chrome painting
    if (d->mode == Mode::LivePreview) {
        QPointF docPos = e->position() + QPointF(d->horizontalOffset(), d->verticalOffset());
        QTextCursor tc = d->control->cursorForPosition(docPos);
        QTextTable *table = tc.currentTable();

        int newRow = -1;
        int newCol = -1;
        if (table) {
            QTextTableCell cell = table->cellAt(tc);
            newRow = cell.row();
            newCol = cell.column();
        }

        if (table != d->hoverTable || newRow != d->hoverTableRow || newCol != d->hoverTableCol) {
            d->hoverTable = table;
            d->hoverTableRow = newRow;
            d->hoverTableCol = newCol;
            viewport()->update();
        }
    }

    if (!(e->buttons() & Qt::LeftButton))
        return;
    if (e->source() == Qt::MouseEventNotSynthesized) {
        const QRect visible = viewport()->rect();
        if (visible.contains(pos))
            d->autoScrollTimer.stop();
        else if (!d->autoScrollTimer.isActive())
            d->autoScrollTimer.start(100, this);
    }
}

void Editor::mouseReleaseEvent(QMouseEvent *e)
{
    d->mouseDragging = false;
    d->sendControlEvent(e);

    // Apply deferred highlighter update after drag selection.
    // Note: do NOT reparse here — if this is a drag-drop, the text
    // hasn't moved yet (dropEvent fires AFTER mouseRelease).
    if (d->mode == Mode::LivePreview) {
        int cursorBlockNum = d->control->textCursor().block().blockNumber();
        d->highlighter->setCursorPosition(cursorBlockNum,
                                           d->control->textCursor().positionInBlock());
        d->updateBlockDisplayModes();
    }

    if (e->source() == Qt::MouseEventNotSynthesized && d->autoScrollTimer.isActive()) {
        d->autoScrollTimer.stop();
        d->ensureCursorVisible();
    }
    d->clickCausedFocus = 0;
}

void Editor::mouseDoubleClickEvent(QMouseEvent *e)
{
    d->sendControlEvent(e);
}

void Editor::focusInEvent(QFocusEvent *e)
{
    if (e->reason() == Qt::MouseFocusReason) {
        d->clickCausedFocus = 1;
    }
    QAbstractScrollArea::focusInEvent(e);
    d->sendControlEvent(e);

    // Ensure cursor blink starts on first show
    if (d->showCursorOnInitialShow) {
        d->showCursorOnInitialShow = 0;
        d->control->setCursorVisible(true);
    }
}

void Editor::focusOutEvent(QFocusEvent *e)
{
    QAbstractScrollArea::focusOutEvent(e);
    d->sendControlEvent(e);
}

void Editor::inputMethodEvent(QInputMethodEvent *e)
{
    d->sendControlEvent(e);
    const bool emptyEvent = e->preeditString().isEmpty() && e->commitString().isEmpty()
                         && e->attributes().isEmpty();
    if (emptyEvent)
        return;
    ensureCursorVisible();
}

void Editor::scrollContentsBy(int /*dx*/, int /*dy*/)
{
    // Pixel-based scrolling: the scrollbar value already changed.
    // Just repaint the viewport.
    viewport()->update();
}

QVariant Editor::inputMethodQuery(Qt::InputMethodQuery property) const
{
    QVariant v;
    switch (property) {
    case Qt::ImEnabled:
        return isEnabled();
    case Qt::ImHints:
    case Qt::ImInputItemClipRectangle:
        return QWidget::inputMethodQuery(property);
    default:
        break;
    }

    const QPointF offset = Markoff::contentOffset(d.get(), this);
    QVariant argument;
    v = d->control->inputMethodQuery(property, argument);
    switch (v.userType()) {
    case QMetaType::QRectF:
        return v.toRectF().translated(offset);
    case QMetaType::QPointF:
        return v.toPointF() + offset;
    case QMetaType::QRect:
        return v.toRect().translated(offset.toPoint());
    case QMetaType::QPoint:
        return v.toPoint() + offset.toPoint();
    default:
        break;
    }
    return v;
}

void Editor::contextMenuEvent(QContextMenuEvent *e)
{
    if (d->mode == Mode::LivePreview) {
        QPointF docPos = QPointF(e->pos()) + QPointF(d->horizontalOffset(), d->verticalOffset());
        QTextCursor tc = d->control->cursorForPosition(docPos);
        QTextTable *table = tc.currentTable();
        if (table) {
            QTextTableCell cell = table->cellAt(tc);
            int row = cell.row();
            int col = cell.column();

            QMenu menu(this);
            menu.addAction(tr("Insert Row Above"), this, [this, table, row]() {
                table->insertRows(row, 1);
                viewport()->update();
            });
            menu.addAction(tr("Insert Row Below"), this, [this, table, row]() {
                table->insertRows(row + 1, 1);
                viewport()->update();
            });
            menu.addSeparator();
            menu.addAction(tr("Insert Column Before"), this, [this, table, col]() {
                table->insertColumns(col, 1);
                viewport()->update();
            });
            menu.addAction(tr("Insert Column After"), this, [this, table, col]() {
                table->insertColumns(col + 1, 1);
                viewport()->update();
            });
            menu.addSeparator();
            if (table->rows() > 1) {
                menu.addAction(tr("Delete Row"), this, [this, table, row]() {
                    table->removeRows(row, 1);
                    viewport()->update();
                });
            }
            if (table->columns() > 1) {
                menu.addAction(tr("Delete Column"), this, [this, table, col]() {
                    table->removeColumns(col, 1);
                    viewport()->update();
                });
            }

            menu.exec(e->globalPos());
            e->accept();
            return;
        }
    }

    // Default context menu for non-table areas
    d->sendControlEvent(e);
}

void Editor::dragEnterEvent(QDragEnterEvent *e)
{
    d->inDrag = true;
    d->sendControlEvent(e);
}

void Editor::dragMoveEvent(QDragMoveEvent *e)
{
    d->autoScrollDragPos = e->position().toPoint();
    if (!d->autoScrollTimer.isActive())
        d->autoScrollTimer.start(100, this);
    d->sendControlEvent(e);
}

void Editor::dropEvent(QDropEvent *e)
{
    d->inDrag = false;
    d->autoScrollTimer.stop();
    d->sendControlEvent(e);
    // Reparse handled by deferred textChanged handler
}

bool Editor::event(QEvent *e)
{
    switch (e->type()) {
#ifndef QT_NO_CONTEXTMENU
    case QEvent::ContextMenu:
        if (static_cast<QContextMenuEvent *>(e)->reason() == QContextMenuEvent::Keyboard) {
            ensureCursorVisible();
            const QPoint cursorPos = cursorRect().center();
            QContextMenuEvent ce(QContextMenuEvent::Keyboard, cursorPos, viewport()->mapToGlobal(cursorPos));
            ce.setAccepted(e->isAccepted());
            const bool result = QAbstractScrollArea::event(&ce);
            e->setAccepted(ce.isAccepted());
            return result;
        }
        break;
#endif
    case QEvent::ShortcutOverride:
    case QEvent::ToolTip:
        d->sendControlEvent(e);
        break;
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        d->control->setPalette(palette());
        break;
    default:
        break;
    }
    return QAbstractScrollArea::event(e);
}

} // namespace Markoff

#include "Editor.moc"
