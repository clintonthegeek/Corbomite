// SPDX-License-Identifier: GPL-3.0-or-later
// Forked from Qt's QPlainTextEdit (QPlainTextEditPrivate)
// Original: Copyright (C) The Qt Company Ltd. (GPL-2.0-only OR GPL-3.0-only)

#ifndef MARKOFF_EDITOR_P_H
#define MARKOFF_EDITOR_P_H

#include <markoff/Editor.h>
#include <markoff/Document.h>
#include <markoff/Renderer.h>
#include "TextControl.h"
#include "DecoratedRange.h"
#include "TableHandler.h"
#include "MarkdownHighlighter.h"
#include "TreeSitterParser.h"

#include <QHash>
#include <QList>
#include <Qt>
#include <QtGui/qtexttable.h>

#include <QtGui/qtextdocumentfragment.h>
#include <QtWidgets/qscrollbar.h>
#include <QtGui/qtextcursor.h>
#include <QtGui/qtextformat.h>
#include <QtWidgets/qmenu.h>
#include <QtGui/qabstracttextdocumentlayout.h>
#include <QtCore/qbasictimer.h>
#include <QtCore/qpointer.h>

namespace Markoff {

class PlainTextDocumentLayout;
class EditorControl;

// Editor::Private is declared as a private nested struct in Editor.h,
// but we define it here. Since Editor.h uses std::unique_ptr<Private>,
// this definition is needed in the .cpp files.
struct Editor::Private {
    Editor *q = nullptr;

    void init(const QString &txt = QString());
    void repaintContents(const QRectF &contentsRect);

    QPoint mapToContents(const QPoint &point) const
    {
        return QPoint(point.x() + horizontalOffset(), point.y() + verticalOffset());
    }

    void adjustScrollbars();
    void verticalScrollbarActionTriggered(int action);
    void ensureViewportLayouted();
    void relayoutDocument();

    void pageUpDown(QTextCursor::MoveOperation op, QTextCursor::MoveMode moveMode, bool moveCursor = true);

    int horizontalOffset() const
    {
        auto *hbar = q->horizontalScrollBar();
        return (q->isRightToLeft() ? (hbar->maximum() - hbar->value()) : hbar->value());
    }

    qreal verticalOffset(int topBlock, int topLine) const;
    qreal verticalOffset() const;

    void sendControlEvent(QEvent *e); // defined in Editor.cpp (EditorControl is incomplete here)

    void updateDefaultTextOption();

    QBasicTimer autoScrollTimer;
    QPoint autoScrollDragPos;

    EditorControl *control = nullptr;
    MarkdownHighlighter *highlighter = nullptr;
    qreal topLineFracture = 0;
    qreal pageUpDownLastCursorY = 0;
    QTextOption::WrapMode wordWrap = QTextOption::WrapAtWordBoundaryOrAnywhere;
    int originalOffsetY = 0;
    int topLine = 0;

    uint tabChangesFocus : 1 = 0;
    uint showCursorOnInitialShow : 1 = 0;
    uint backgroundVisible : 1 = 0;
    uint centerOnScroll : 1 = 0;
    uint inDrag : 1 = 0;
    uint clickCausedFocus : 1 = 0;
    uint pageUpDownLastCursorYIsValid : 1 = 0;

    void setTopLine(int visualTopLine, int dx = 0);
    void setTopBlock(int newTopBlock, int newTopLine, int dx = 0);

    void ensureVisible(int position, bool center, bool forceCenter = false);
    void ensureCursorVisible(bool center = false);
    void updateViewport();

    QPointer<PlainTextDocumentLayout> documentLayoutPtr;

    // Live preview
    Editor::Mode mode = Editor::Mode::Source;
    std::unique_ptr<Markoff::Document> parsedDoc;
    Markoff::Renderer renderer;
    Markoff::TreeSitterParser tsParser;
    bool needsReparse = false;
    bool inReparse = false;  // guard against reparse loops from highlighter
    bool mouseDragging = false;  // suppress highlighter updates during drag

    void reparseDocument();
    void applyBlockFormats();  // set QTextBlockFormat (margins, indent) from span map
    void updateBlockDisplayModes();
    void renderBlock(QTextBlock &block);

    // Decorated ranges (code blocks, callouts — text with visual chrome)
    QList<DecoratedRange> decoratedRanges;
    void detectDecoratedRanges();
    const DecoratedRange *decoratedRangeAt(int blockNumber) const;

    // Live tables (QTextTable objects that replaced pipe text)
    QList<QTextTable *> liveTables;
    QList<QList<Qt::Alignment>> tableAlignments;
    void convertTables();
    void revertTables();

    void cursorPositionChanged();
    void modificationChanged(bool);
};

} // namespace Markoff

#endif // MARKOFF_EDITOR_P_H
