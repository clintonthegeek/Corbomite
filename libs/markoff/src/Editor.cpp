// SPDX-License-Identifier: GPL-3.0-or-later
// Scrolling model follows QTextEdit exactly (pixel-based, QTextDocumentLayout).
// Original QPlainTextEdit fork removed — QTextDocument's default layout handles
// QTextTable, pixel scrolling, variable heights, and hit testing natively.

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

/// Check if a block is inside a QTextTable — no cursor creation,
/// no recursion risk. Checks if the block's position falls within
/// any child frame of the root frame (child frames are QTextTables).
static bool isTableCellBlock(const QTextBlock &block)
{
    const QTextDocument *doc = block.document();
    if (!doc) return false;
    int pos = block.position();
    const auto childFrames = doc->rootFrame()->childFrames();
    for (QTextFrame *frame : childFrames) {
        if (pos >= frame->firstPosition() && pos <= frame->lastPosition())
            return true;
    }
    return false;
}

static QTextTable *tableForBlock(const QTextBlock &block)
{
    QTextCursor cursor(block);
    return cursor.currentTable();
}

static bool isFirstTableBlock(const QTextBlock &block, QTextTable *table)
{
    if (!table) return false;
    QTextTableCell firstCell = table->cellAt(0, 0);
    return firstCell.firstCursorPosition().block() == block;
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
    // Walk blocks using the layout's absolute positions to find
    // the first block visible at the current scroll offset.
    QAbstractTextDocumentLayout *layout = document()->documentLayout();
    qreal scrollY = textEdit->verticalScrollBar()->value();
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        QRectF r = layout->blockBoundingRect(block);
        if (r.top() + r.height() > scrollY)
            return block;
        block = block.next();
    }
    return document()->lastBlock();
}

int EditorControl::hitTest(const QPointF &point, Qt::HitTestAccuracy accuracy) const {
    // QTextDocumentLayout has its own hitTest. The point is already in
    // document coordinates (processEvent transforms it).
    return document()->documentLayout()->hitTest(point, accuracy);
}

QRectF EditorControl::blockBoundingRect(const QTextBlock &block) const {
    // QTextDocumentLayout positions blocks with absolute Y offsets.
    return document()->documentLayout()->blockBoundingRect(block);
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

    // QTextDocument creates a QTextDocumentLayout by default.
    // We use it as-is — no custom layout needed.
    QTextDocument *doc = new QTextDocument(control);
    control->setDocument(doc);

    highlighter = new MarkdownHighlighter(doc);

    control->setPalette(q->palette());

    QObject::connect(control, &TextControl::microFocusChanged, q,
                     [this]() { q->updateMicroFocus(); });
    QObject::connect(control, &TextControl::documentSizeChanged, q,
                     [this](const QSizeF &) { adjustScrollbars(); });
    QObject::connect(control, &TextControl::updateRequest, q,
                     [this](const QRectF &rect) { repaintContents(rect); });
    QObject::connect(control, &TextControl::textChanged, q, &Editor::textChanged);
    QObject::connect(control, &TextControl::textChanged, q, [this]() { q->updateMicroFocus(); });

    // Live preview: re-parse on text changes.
    // Debounced: wait 150ms after the last keystroke before reparsing.
    // This avoids full-document reparse on every character typed.
    // The reparse (md4c + tree-sitter + rehighlight + block walks)
    // is O(N) over the whole document — too expensive per keystroke.
    auto *reparseTimer = new QTimer(q);
    reparseTimer->setSingleShot(true);
    reparseTimer->setInterval(150);
    QObject::connect(control, &TextControl::textChanged, q, [this, reparseTimer]() {
        if (mode == Editor::Mode::LivePreview && !inReparse) {
            needsReparse = true;
            highlighter->setSpanMapStale(true);  // freeze formatting until reparse
            reparseTimer->start();  // restart 150ms countdown
        }
    });
    QObject::connect(reparseTimer, &QTimer::timeout, q, [this]() {
                if (!needsReparse) return;
                needsReparse = false;
                inReparse = true;
                highlighter->setCursorPosition(
                    control->textCursor().block().blockNumber(),
                    control->textCursor().positionInBlock());
                reparseDocument();  // revert tables, parse, highlight setup, block formats
                highlighter->setSpanMapStale(false);  // span map is fresh now
                highlighter->rehighlight();
                updateBlockDisplayModes();
                // Convert tables as the VERY LAST step — after all block
                // iteration (rehighlight, updateBlockDisplayModes, applyBlockFormats)
                // is done. These functions walk all blocks and would corrupt
                // QTextTable cell structure if tables existed during their run.
                convertTables();
                // Clear inReparse on the NEXT event loop iteration,
                // after any deferred rehighlight format changes have
                // fired their textChanged signals (still suppressed).
                QTimer::singleShot(0, q, [this]() {
                    inReparse = false;
                    needsReparse = false;
                });
    });

    // Live preview: update display modes and highlighter on cursor movement.
    // Suppressed during mouse drag to avoid per-pixel rehighlighting.
    QObject::connect(control, &TextControl::cursorPositionChanged, q, [this]() {
        if (mouseDragging || inReparse)
            return;

        // Temporarily suppress textChanged -> reparse during display mode
        // updates, because setUserData() triggers documentChanged.
        bool wasInReparse = inReparse;
        inReparse = true;

        if (mode == Editor::Mode::LivePreview) {
            int cursorBlockNum = control->textCursor().block().blockNumber();
            highlighter->setCursorPosition(cursorBlockNum, control->textCursor().positionInBlock());
        }
        updateBlockDisplayModes();
        inReparse = wasInReparse;
    });

    // Use setPageSize like QTextEdit — initially null to avoid relayouting
    // until the widget is shown. relayoutDocument() sets it to viewport width.
    doc->setPageSize(QSize(0, 0));
    doc->documentLayout()->setPaintDevice(q->viewport());
    doc->setDefaultFont(q->font());

    if (!txt.isEmpty())
        control->setPlainText(txt);

    q->horizontalScrollBar()->setSingleStep(20);
    q->verticalScrollBar()->setSingleStep(20);

    q->viewport()->setBackgroundRole(QPalette::Base);
    q->setAcceptDrops(true);
    q->setFocusPolicy(Qt::StrongFocus);
    q->setAttribute(Qt::WA_KeyCompression);
    q->setAttribute(Qt::WA_InputMethodEnabled);

    // Ensure cursor is visible and blinking when the widget gets focus.
    showCursorOnInitialShow = 1;
    q->setInputMethodHints(Qt::ImhMultiLine);

#ifndef QT_NO_CURSOR
    q->viewport()->setCursor(Qt::IBeamCursor);
#endif
}

void Editor::Private::cursorPositionChanged()
{
    // Track active atomic block and update highlighter cursor
    if (mode == Editor::Mode::LivePreview) {
        int cursorBlockNum = control->textCursor().block().blockNumber();
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
    const QString text = q->toPlainText();
    parsedDoc = Document::fromMarkdown(text);

    // Parse the exact editor text with tree-sitter for the highlighter.
    // Tree-sitter produces a CST with explicit delimiter nodes and byte
    // offsets that match the QTextDocument positions exactly.
    tsParser.parse(text);
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
        // Trim trailing newline — the frame boundary replacement
        // already provides block separation. An extra newline adds
        // a blank block that accumulates on each revert/convert cycle.
        if (md.endsWith(QLatin1Char('\n')))
            md.chop(1);

        // To remove a QTextTable frame entirely (not just empty its cells),
        // we must select from OUTSIDE the frame boundaries.
        int frameStart = tt->firstPosition() - 1;
        int frameEnd = tt->lastPosition() + 1;

        QTextCursor cursor(q->document());
        cursor.setPosition(frameStart);
        cursor.setPosition(frameEnd, QTextCursor::KeepAnchor);
        cursor.beginEditBlock();
        cursor.insertText(md);
        cursor.endEditBlock();
    }
    liveTables.clear();
    tableAlignments.clear();
    hoverTable = nullptr;
}

void Editor::Private::checkTableCreationTrigger()
{
    if (mode != Editor::Mode::LivePreview)
        return;
    QTextCursor tc = control->textCursor();
    QTextBlock currentBlock = tc.block();
    QString currentText = currentBlock.text().trimmed();

    static const QRegularExpression separatorRe(
        QStringLiteral(R"(^\s*\|[\s:]*-+[\s:]*(\|[\s:]*-+[\s:]*)*\|\s*$)"));
    if (!separatorRe.match(currentText).hasMatch())
        return;

    QTextBlock prevBlock = currentBlock.previous();
    if (!prevBlock.isValid())
        return;
    static const QRegularExpression pipeRowRe(
        QStringLiteral(R"(^\s*\|.*\|\s*$)"));
    if (!pipeRowRe.match(prevBlock.text()).hasMatch())
        return;

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
        QTextTableCell dataCell = tt->cellAt(1, 0);
        control->setTextCursor(dataCell.firstCursorPosition());
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
        if (isTableCellBlock(block)) {
            block = block.next();
            continue;
        }

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
    auto blockDoc = Document::fromMarkdown(blockText);
    auto rendered = renderer.renderToTextDocument(*blockDoc);

    rendered->setDocumentMargin(q->document()->documentMargin());

    // Strip paragraph margins from rendered blocks
    QTextBlock renderedBlock = rendered->begin();
    while (renderedBlock.isValid()) {
        QTextCursor c(renderedBlock);
        QTextBlockFormat fmt = renderedBlock.blockFormat();
        fmt.setTopMargin(0);
        fmt.setBottomMargin(0);
        c.setBlockFormat(fmt);
        renderedBlock = renderedBlock.next();
    }

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
// Decorated range detection
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

// ============================================================================
// Scrolling — follows QTextEdit exactly (pixel-based)
// ============================================================================

void Editor::Private::adjustScrollbars()
{
    if (ignoreAutomaticScrollbarAdjustment)
        return;
    ignoreAutomaticScrollbarAdjustment = true;

    auto *vbar = q->verticalScrollBar();
    auto *hbar = q->horizontalScrollBar();
    auto *vp = q->viewport();
    QAbstractTextDocumentLayout *layout = control->document()->documentLayout();

    QSize viewportSize = vp->size();
    QSizeF docSize = layout->documentSize();

    // Iterate like QTextEdit to stabilize scrollbar/viewport sizes
    for (int i = 0; i < 4; ++i) {
        hbar->setRange(0, qMax(0, static_cast<int>(docSize.width()) - viewportSize.width()));
        hbar->setPageStep(viewportSize.width());

        vbar->setRange(0, qMax(0, static_cast<int>(docSize.height()) - viewportSize.height()));
        vbar->setPageStep(viewportSize.height());

        if (q->isRightToLeft())
            vp->update();

        const QSize oldViewportSize = viewportSize;
        viewportSize = vp->size();
        if (viewportSize.width() != oldViewportSize.width())
            relayoutDocument();

        QSizeF newDocSize = layout->documentSize();
        if (viewportSize == oldViewportSize
            && qFuzzyCompare(newDocSize.width(), docSize.width())
            && qFuzzyCompare(newDocSize.height(), docSize.height()))
            break;
        docSize = newDocSize;
    }

    ignoreAutomaticScrollbarAdjustment = false;
}

void Editor::Private::ensureVisible(const QRectF &_rect)
{
    const QRect rect = _rect.toRect();
    auto *vbar = q->verticalScrollBar();
    auto *hbar = q->horizontalScrollBar();
    auto *vp = q->viewport();

    if ((vbar->isVisible() && vbar->maximum() < rect.bottom())
        || (hbar->isVisible() && hbar->maximum() < rect.right()))
        adjustScrollbars();

    const int visibleWidth = vp->width();
    const int visibleHeight = vp->height();
    const bool rtl = q->isRightToLeft();

    if (rect.x() < horizontalOffset()) {
        if (rtl)
            hbar->setValue(hbar->maximum() - rect.x());
        else
            hbar->setValue(rect.x());
    } else if (rect.x() + rect.width() > horizontalOffset() + visibleWidth) {
        if (rtl)
            hbar->setValue(hbar->maximum() - (rect.x() + rect.width() - visibleWidth));
        else
            hbar->setValue(rect.x() + rect.width() - visibleWidth);
    }

    if (rect.y() < verticalOffset())
        vbar->setValue(rect.y());
    else if (rect.y() + rect.height() > verticalOffset() + visibleHeight)
        vbar->setValue(rect.y() + rect.height() - visibleHeight);
}

void Editor::Private::ensureCursorVisible()
{
    // TextControl::ensureCursorVisible emits visibilityRequest(crect) which
    // we handle via the ensureVisible(QRectF) method above if we connect it.
    // But for simplicity, compute cursor rect in document coordinates and scroll.
    QRectF crect = control->cursorRect();
    crect = crect.adjusted(-5, 0, 5, 0);
    ensureVisible(crect);
}

void Editor::Private::updateViewport()
{
    q->viewport()->update();
}

void Editor::Private::repaintContents(const QRectF &contentsRect)
{
    if (!contentsRect.isValid()) {
        q->viewport()->update();
        return;
    }
    const int xOffset = horizontalOffset();
    const int yOffset = verticalOffset();
    const QRectF visibleRect(xOffset, yOffset, q->viewport()->width(), q->viewport()->height());

    QRect r = contentsRect.intersected(visibleRect).toAlignedRect();
    if (r.isEmpty())
        return;

    r.translate(-xOffset, -yOffset);
    q->viewport()->update(r);
}

void Editor::Private::pageUpDown(QTextCursor::MoveOperation op, QTextCursor::MoveMode moveMode)
{
    // Follows QTextEdit's pageUpDown exactly
    QTextCursor cursor = control->textCursor();
    bool moved = false;
    qreal lastY = control->cursorRect(cursor).top();
    qreal distance = 0;
    do {
        qreal y = control->cursorRect(cursor).top();
        distance += qAbs(y - lastY);
        lastY = y;
        moved = cursor.movePosition(op, moveMode);
    } while (moved && distance < q->viewport()->height());

    if (moved) {
        if (op == QTextCursor::Up) {
            cursor.movePosition(QTextCursor::Down, moveMode);
            q->verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
        } else {
            cursor.movePosition(QTextCursor::Up, moveMode);
            q->verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
        }
    }
    control->setTextCursor(cursor, moveMode == QTextCursor::KeepAnchor);
}

void Editor::Private::relayoutDocument()
{
    QTextDocument *doc = control->document();

    const bool oldIgnore = ignoreAutomaticScrollbarAdjustment;
    ignoreAutomaticScrollbarAdjustment = true;

    int width = q->viewport()->width();
    doc->setPageSize(QSize(width, -1));

    ignoreAutomaticScrollbarAdjustment = oldIgnore;
    adjustScrollbars();
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
    static constexpr qreal livePreviewMargin = 20.0;

    if (m == Mode::LivePreview) {
        document()->setDocumentMargin(livePreviewMargin);

        // Update the renderer to use zero internal margin (the document
        // margin handles it) so rendered blocks align with raw text
        RenderSettings settings = d->renderer.settings();
        settings.marginPx = 0;
        d->renderer.setSettings(settings);

        // Parse first so spans are available, THEN switch mode.
        d->inReparse = true;
        d->reparseDocument();
        d->highlighter->setCursorPosition(d->control->textCursor().block().blockNumber(),
                                          d->control->textCursor().positionInBlock());
        d->highlighter->setMode(MarkdownHighlighter::Mode::LivePreview);
        d->updateBlockDisplayModes();
        // Convert tables LAST, after all block iteration is complete
        d->convertTables();
        // Clear inReparse on next event loop, after deferred rehighlight
        QTimer::singleShot(0, this, [this]() {
            d->inReparse = false;
            d->needsReparse = false;
        });
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
    d->ensureCursorVisible();
}

QRect Editor::cursorRect() const
{
    QRectF r = d->control->cursorRect();
    r.translate(-d->horizontalOffset(), -d->verticalOffset());
    return r.toRect();
}

// ============================================================================
// Paint — follows QTextEdit: translate painter, then drawContents
// ============================================================================

void Editor::paintTable(QPainter *painter, QTextTable *table,
                        const QRectF &tableRect, const QRect &/*viewportRect*/)
{
    // The painter is already translated to document coordinates.
    // tableRect is in document coordinates from frameBoundingRect.
    const int rows = table->rows();
    const int cols = table->columns();

    const qreal tableX = tableRect.left();
    const qreal tableY = tableRect.top();
    const qreal tableW = tableRect.width();
    const qreal tableH = tableRect.height();

    painter->save();

    // Background
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0xfa, 0xfa, 0xfa));
    painter->drawRect(tableRect);

    // Grid lines — compute from cell geometry
    // QTextDocumentLayout positions cells; we get cell rects from the table format
    // For simplicity, draw lines between rows/columns using cell positions
    painter->setPen(QPen(QColor(0xe0, 0xe0, 0xe0), 1));

    // Outer border
    painter->drawRect(tableRect);

    // Horizontal lines between rows
    for (int r = 1; r < rows; ++r) {
        QTextTableCell cell = table->cellAt(r, 0);
        QRectF cellRect = document()->documentLayout()->blockBoundingRect(
            cell.firstCursorPosition().block());
        // The cell block's Y is the row's Y in document coords
        // We need to get the row start from the table frame
        // Use the first cell of each row to find row boundaries
        qreal y = cellRect.top();
        // For the header separator, use thicker line
        if (r == 1) {
            painter->save();
            painter->setPen(QPen(QColor(0xc0, 0xc0, 0xc0), 2));
            painter->drawLine(QPointF(tableX, y), QPointF(tableX + tableW, y));
            painter->restore();
        } else {
            painter->drawLine(QPointF(tableX, y), QPointF(tableX + tableW, y));
        }
    }

    // Vertical lines between columns (approximate from first row cell positions)
    for (int c = 1; c < cols; ++c) {
        QTextTableCell cell = table->cellAt(0, c);
        QRectF cellRect = document()->documentLayout()->blockBoundingRect(
            cell.firstCursorPosition().block());
        qreal x = cellRect.left();
        painter->drawLine(QPointF(x, tableY), QPointF(x, tableY + tableH));
    }

    // Cursor highlight: if cursor is inside this table, highlight the cell
    QTextCursor tc = d->control->textCursor();
    QTextTable *cursorTable = tc.currentTable();
    if (cursorTable == table) {
        QTextTableCell curCell = table->cellAt(tc);
        if (curCell.isValid()) {
            QRectF cellBR = document()->documentLayout()->blockBoundingRect(
                curCell.firstCursorPosition().block());
            // Approximate cell rect
            painter->setPen(QPen(palette().color(QPalette::Highlight), 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(cellBR.adjusted(1, 1, -1, -1));
        }
    }

    painter->restore();
}

void Editor::paintEvent(QPaintEvent *e)
{
    QPainter painter(viewport());

    const int xOffset = d->horizontalOffset();
    const int yOffset = d->verticalOffset();

    QRect er = e->rect();

    // Translate to document coordinates (like QTextEdit)
    painter.translate(-xOffset, -yOffset);
    er.translate(xOffset, yOffset);

    // Paint decorated range backgrounds BEFORE text (in document coordinates)
    if (d->mode == Mode::LivePreview) {
        paintDecoratedRangeBackgrounds(&painter, er);
    }

    // Paint all document content (text, tables, inline objects)
    // QTextDocumentLayout::draw() handles everything including QTextTable
    d->control->drawContents(&painter, er, this);

    // Paint table chrome ON TOP (grid lines, hover handles)
    if (d->mode == Mode::LivePreview) {
        paintTableChrome(&painter, er);
    }
}

void Editor::paintDecoratedRangeBackgrounds(QPainter *painter, const QRect &docRect)
{
    QAbstractTextDocumentLayout *layout = document()->documentLayout();

    for (const DecoratedRange &dr : d->decoratedRanges) {
        // Compute the range's bounding rect in document coordinates
        QTextBlock firstBlock = document()->findBlockByNumber(dr.firstBlock);
        QTextBlock lastBlock = document()->findBlockByNumber(dr.lastBlock);
        if (!firstBlock.isValid()) continue;

        QRectF firstBR = layout->blockBoundingRect(firstBlock);
        qreal rangeTop = firstBR.top();
        qreal rangeHeight = 0;
        QTextBlock b = firstBlock;
        for (int i = dr.firstBlock; i <= dr.lastBlock && b.isValid(); ++i, b = b.next())
            rangeHeight += layout->blockBoundingRect(b).height();

        QRectF rangeRect(firstBR.left(), rangeTop, firstBR.width(), rangeHeight);
        if (!rangeRect.intersects(QRectF(docRect)))
            continue;

        qreal margin = document()->documentMargin();
        QRectF bgRect(margin - 4, rangeTop,
                      viewport()->width() - margin * 2 + 8, rangeHeight);

        if (dr.type == DecoratedRange::CodeBlock) {
            painter->save();
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0xf5, 0xf5, 0xf5));
            painter->setRenderHint(QPainter::Antialiasing);
            painter->drawRoundedRect(bgRect, 4, 4);
            // Border
            painter->setPen(QPen(QColor(0xe0, 0xe0, 0xe0), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            // Language label
            if (!dr.language.isEmpty()) {
                QFont labelFont = font();
                labelFont.setPointSize(qMax(8, font().pointSize() - 2));
                painter->setFont(labelFont);
                painter->setPen(QColor(0x9e, 0x9e, 0x9e));
                QRectF labelRect(bgRect.right() - 80, bgRect.top() + 2, 72, 16);
                painter->drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, dr.language);
            }
            painter->restore();
        } else if (dr.type == DecoratedRange::Callout) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            QColor bg = dr.calloutColor;
            bg.setAlpha(20);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bg);
            painter->drawRoundedRect(bgRect, 4, 4);
            // Left border
            painter->setBrush(dr.calloutColor);
            painter->drawRoundedRect(QRectF(bgRect.left(), bgRect.top(), 4, bgRect.height()), 2, 2);
            painter->restore();
        }

        // Blockquote vertical lines for blocks in this range
        QTextBlock blk = firstBlock;
        for (int i = dr.firstBlock; i <= dr.lastBlock && blk.isValid(); ++i, blk = blk.next()) {
            qreal leftMargin = blk.blockFormat().leftMargin();
            if (leftMargin > 0) {
                const auto &spans = d->highlighter->spans();
                int bqDepth = 0;
                int blockStart = blk.position();
                int blockEnd = blockStart + blk.length();
                for (const SourceSpan &s : spans) {
                    int se = s.charOffset + s.charLength;
                    if (se <= blockStart) continue;
                    if (s.charOffset >= blockEnd) break;
                    if (s.blockquoteDepth > bqDepth)
                        bqDepth = s.blockquoteDepth;
                }

                if (bqDepth > 0) {
                    bool isCallout = dr.type == DecoratedRange::Callout;
                    if (!isCallout) {
                        QRectF blkR = layout->blockBoundingRect(blk);
                        painter->save();
                        QFontMetricsF fm(font());
                        qreal chevronWidth = qCeil(fm.horizontalAdvance(QStringLiteral("> ")));
                        QColor lineColor = d->highlighter->blockquoteColor().lighter(140);
                        painter->setPen(QPen(lineColor, 2));
                        for (int level = 0; level < bqDepth; ++level) {
                            qreal x = margin + level * chevronWidth + 1;
                            painter->drawLine(QPointF(x, blkR.top()),
                                            QPointF(x, blkR.top() + blkR.height()));
                        }
                        painter->restore();
                    }
                }
            }
        }
    }

    // Paint blockquote vertical lines for blocks NOT in any decorated range
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        if (d->mode == Mode::LivePreview && !d->decoratedRangeAt(block.blockNumber())) {
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
                    QRectF blkR = layout->blockBoundingRect(block);
                    if (blkR.intersects(QRectF(docRect))) {
                        painter->save();
                        qreal margin = document()->documentMargin();
                        QFontMetricsF fm(font());
                        qreal chevronWidth = qCeil(fm.horizontalAdvance(QStringLiteral("> ")));
                        QColor lineColor = d->highlighter->blockquoteColor().lighter(140);
                        painter->setPen(QPen(lineColor, 2));
                        for (int level = 0; level < bqDepth; ++level) {
                            qreal x = margin + level * chevronWidth + 1;
                            painter->drawLine(QPointF(x, blkR.top()),
                                            QPointF(x, blkR.top() + blkR.height()));
                        }
                        painter->restore();
                    }
                }
            }
        }

        // Horizontal rule painting
        if (d->mode == Mode::LivePreview) {
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
                QRectF r = layout->blockBoundingRect(block);
                if (r.intersects(QRectF(docRect))) {
                    painter->save();
                    qreal margin = document()->documentMargin();
                    qreal y = r.top() + r.height() / 2;
                    qreal x1 = margin;
                    qreal x2 = viewport()->width() - margin;
                    painter->setPen(QPen(QColor(0xcc, 0xcc, 0xcc), 1));
                    painter->drawLine(QPointF(x1, y), QPointF(x2, y));
                    painter->restore();
                }
            }
        }

        block = block.next();
    }
}

void Editor::paintTableChrome(QPainter *painter, const QRect &docRect)
{
    for (QTextTable *table : d->liveTables) {
        if (!table) continue;
        QRectF tableRect = document()->documentLayout()->frameBoundingRect(table);
        if (!tableRect.intersects(QRectF(docRect)))
            continue;
        paintTable(painter, table, tableRect, docRect);
    }
}

// ============================================================================
// Event handlers
// ============================================================================

void Editor::resizeEvent(QResizeEvent *e)
{
    if (e->oldSize().width() != e->size().width())
        d->relayoutDocument();
    else
        d->adjustScrollbars();
}

void Editor::keyPressEvent(QKeyEvent *e)
{
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

    if (e->text() == QStringLiteral("|") && d->mode == Mode::LivePreview)
        d->checkTableCreationTrigger();
}

void Editor::mousePressEvent(QMouseEvent *e)
{
    d->mouseDragging = true;
    d->sendControlEvent(e);
}

void Editor::mouseMoveEvent(QMouseEvent *e)
{
    d->inDrag = false;
    const QPoint pos = e->position().toPoint();
    d->sendControlEvent(e);
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

void Editor::scrollContentsBy(int dx, int dy)
{
    Q_UNUSED(dy);
    if (isRightToLeft())
        dx = -dx;
    viewport()->scroll(dx, dy);
    QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle | Qt::ImAnchorRectangle);
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

    const QPointF offset(-d->horizontalOffset(), -d->verticalOffset());
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
        QPointF contentPos = QPointF(e->pos()) + QPointF(d->horizontalOffset(), d->verticalOffset());
        QTextCursor tc = d->control->cursorForPosition(contentPos);
        QTextTable *table = tc.currentTable();
        if (table) {
            QTextTableCell cell = table->cellAt(tc);
            int row = cell.row();
            int col = cell.column();
            QMenu menu(this);
            menu.addAction(tr("Insert Row Above"), this, [this, table, row]() {
                table->insertRows(row, 1); viewport()->update();
            });
            menu.addAction(tr("Insert Row Below"), this, [this, table, row]() {
                table->insertRows(row + 1, 1); viewport()->update();
            });
            menu.addSeparator();
            menu.addAction(tr("Insert Column Before"), this, [this, table, col]() {
                table->insertColumns(col, 1); viewport()->update();
            });
            menu.addAction(tr("Insert Column After"), this, [this, table, col]() {
                table->insertColumns(col + 1, 1); viewport()->update();
            });
            menu.addSeparator();
            if (table->rows() > 1)
                menu.addAction(tr("Delete Row"), this, [this, table, row]() {
                    table->removeRows(row, 1); viewport()->update();
                });
            if (table->columns() > 1)
                menu.addAction(tr("Delete Column"), this, [this, table, col]() {
                    table->removeColumns(col, 1); viewport()->update();
                });
            menu.exec(e->globalPos());
            e->accept();
            return;
        }
    }
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
