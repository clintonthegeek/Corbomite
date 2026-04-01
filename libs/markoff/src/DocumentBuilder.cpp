// SPDX-License-Identifier: GPL-3.0-or-later
#include "DocumentBuilder_p.h"

#include <md4c.h>

namespace Markoff {

DocumentBuilder::DocumentBuilder() = default;

bool DocumentBuilder::parse(const QString &markdown)
{
    m_blocks.clear();
    m_blockStack.clear();
    m_bold = m_italic = m_strikethrough = false;
    m_code = m_math = m_mathDisplay = false;
    m_linkHref.clear();
    m_wikiTarget.clear();

    const QByteArray utf8 = markdown.toUtf8();

    MD_PARSER parser = {};
    parser.abi_version = 0;
    parser.flags = MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS | MD_FLAG_LATEXMATHSPANS;
    parser.enter_block = &DocumentBuilder::onEnterBlock;
    parser.leave_block = &DocumentBuilder::onLeaveBlock;
    parser.enter_span  = &DocumentBuilder::onEnterSpan;
    parser.leave_span  = &DocumentBuilder::onLeaveSpan;
    parser.text        = &DocumentBuilder::onText;
    parser.debug_log   = nullptr;
    parser.syntax      = nullptr;

    int result = md_parse(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), &parser, this);
    return result == 0;
}

QList<Block> DocumentBuilder::takeBlocks()
{
    return std::move(m_blocks);
}

// ---------------------------------------------------------------------------
// Static trampolines
// ---------------------------------------------------------------------------

int DocumentBuilder::onEnterBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    return static_cast<DocumentBuilder *>(userdata)->enterBlock(type, detail);
}

int DocumentBuilder::onLeaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata)
{
    return static_cast<DocumentBuilder *>(userdata)->leaveBlock(type, detail);
}

int DocumentBuilder::onEnterSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    return static_cast<DocumentBuilder *>(userdata)->enterSpan(type, detail);
}

int DocumentBuilder::onLeaveSpan(MD_SPANTYPE type, void *detail, void *userdata)
{
    return static_cast<DocumentBuilder *>(userdata)->leaveSpan(type, detail);
}

int DocumentBuilder::onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata)
{
    return static_cast<DocumentBuilder *>(userdata)->text(type, text, size);
}

// ---------------------------------------------------------------------------
// Instance methods
// ---------------------------------------------------------------------------

int DocumentBuilder::enterBlock(MD_BLOCKTYPE type, void *detail)
{
    Block block;
    block.type = type;

    switch (type) {
    case MD_BLOCK_H: {
        auto *hd = static_cast<MD_BLOCK_H_DETAIL *>(detail);
        block.headingLevel = static_cast<int>(hd->level);
        break;
    }
    case MD_BLOCK_CODE: {
        auto *cd = static_cast<MD_BLOCK_CODE_DETAIL *>(detail);
        if (cd->lang.size > 0)
            block.codeInfo = QString::fromUtf8(cd->lang.text, static_cast<qsizetype>(cd->lang.size));
        break;
    }
    case MD_BLOCK_LI: {
        auto *li = static_cast<MD_BLOCK_LI_DETAIL *>(detail);
        block.isTaskItem = (li->is_task != 0);
        block.taskMark   = li->is_task ? li->task_mark : ' ';
        break;
    }
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
        auto *td = static_cast<MD_BLOCK_TD_DETAIL *>(detail);
        block.tableAlign = td->align;
        break;
    }
    case MD_BLOCK_OL: {
        auto *ol = static_cast<MD_BLOCK_OL_DETAIL *>(detail);
        block.listStart   = static_cast<int>(ol->start);
        block.isTightList = (ol->is_tight != 0);
        break;
    }
    case MD_BLOCK_UL: {
        auto *ul = static_cast<MD_BLOCK_UL_DETAIL *>(detail);
        block.isTightList = (ul->is_tight != 0);
        break;
    }
    default:
        break;
    }

    if (m_blockStack.isEmpty()) {
        m_blocks.append(block);
        m_blockStack.append(&m_blocks.last());
    } else {
        Block *parent = m_blockStack.last();
        parent->children.append(block);
        m_blockStack.append(&parent->children.last());
    }

    return 0;
}

int DocumentBuilder::leaveBlock(MD_BLOCKTYPE /*type*/, void * /*detail*/)
{
    if (!m_blockStack.isEmpty())
        m_blockStack.removeLast();
    return 0;
}

int DocumentBuilder::enterSpan(MD_SPANTYPE type, void *detail)
{
    switch (type) {
    case MD_SPAN_STRONG:
        m_bold = true;
        break;
    case MD_SPAN_EM:
        m_italic = true;
        break;
    case MD_SPAN_DEL:
        m_strikethrough = true;
        break;
    case MD_SPAN_CODE:
        m_code = true;
        break;
    case MD_SPAN_LATEXMATH:
        m_math = true;
        break;
    case MD_SPAN_LATEXMATH_DISPLAY:
        m_mathDisplay = true;
        break;
    case MD_SPAN_A: {
        auto *a = static_cast<MD_SPAN_A_DETAIL *>(detail);
        if (a->href.size > 0)
            m_linkHref = QString::fromUtf8(a->href.text, static_cast<qsizetype>(a->href.size));
        break;
    }
    case MD_SPAN_WIKILINK: {
        auto *wl = static_cast<MD_SPAN_WIKILINK_DETAIL *>(detail);
        if (wl->target.size > 0)
            m_wikiTarget = QString::fromUtf8(wl->target.text, static_cast<qsizetype>(wl->target.size));
        break;
    }
    default:
        break;
    }
    return 0;
}

int DocumentBuilder::leaveSpan(MD_SPANTYPE type, void * /*detail*/)
{
    switch (type) {
    case MD_SPAN_STRONG:
        m_bold = false;
        break;
    case MD_SPAN_EM:
        m_italic = false;
        break;
    case MD_SPAN_DEL:
        m_strikethrough = false;
        break;
    case MD_SPAN_CODE:
        m_code = false;
        break;
    case MD_SPAN_LATEXMATH:
        m_math = false;
        break;
    case MD_SPAN_LATEXMATH_DISPLAY:
        m_mathDisplay = false;
        break;
    case MD_SPAN_A:
        m_linkHref.clear();
        break;
    case MD_SPAN_WIKILINK:
        m_wikiTarget.clear();
        break;
    default:
        break;
    }
    return 0;
}

int DocumentBuilder::text(MD_TEXTTYPE type, const MD_CHAR *rawText, MD_SIZE size)
{
    if (m_blockStack.isEmpty())
        return 0;

    InlineRun run;
    run.bold          = m_bold;
    run.italic        = m_italic;
    run.strikethrough = m_strikethrough;
    run.code          = m_code;
    run.math          = m_math;
    run.mathDisplay   = m_mathDisplay;
    run.linkHref      = m_linkHref;
    run.wikiTarget    = m_wikiTarget;

    switch (type) {
    case MD_TEXT_NORMAL:
    case MD_TEXT_CODE:
    case MD_TEXT_HTML:
    case MD_TEXT_LATEXMATH:
    case MD_TEXT_ENTITY:
        run.text = QString::fromUtf8(rawText, static_cast<qsizetype>(size));
        break;
    case MD_TEXT_BR:
    case MD_TEXT_SOFTBR:
        run.text = QStringLiteral(" ");
        break;
    case MD_TEXT_NULLCHAR:
        run.text = QChar(0xFFFD);
        break;
    }

    if (!run.text.isEmpty())
        m_blockStack.last()->inlines.append(run);

    return 0;
}

} // namespace Markoff
