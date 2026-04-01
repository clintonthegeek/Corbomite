// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_DOCUMENTBUILDER_P_H
#define MARKOFF_DOCUMENTBUILDER_P_H

#include <QString>
#include <QList>
#include <md4c.h>

namespace Markoff {

struct InlineRun {
    QString text;
    bool bold = false;
    bool italic = false;
    bool strikethrough = false;
    bool code = false;
    bool math = false;
    bool mathDisplay = false;
    QString linkHref;
    QString wikiTarget;
};

struct Block {
    MD_BLOCKTYPE type = MD_BLOCK_P;
    int headingLevel = 0;
    QList<InlineRun> inlines;
    QString codeInfo;           // language for code blocks
    MD_ALIGN tableAlign = MD_ALIGN_DEFAULT;
    bool isTaskItem = false;
    MD_CHAR taskMark = ' ';
    int listStart = 1;
    bool isTightList = false;
    QList<Block> children;      // for nested structures
};

class DocumentBuilder {
public:
    DocumentBuilder();
    bool parse(const QString &markdown);
    QList<Block> takeBlocks();

private:
    // Static MD4C callback trampolines
    static int onEnterBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int onLeaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int onEnterSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int onLeaveSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata);

    // Instance methods
    int enterBlock(MD_BLOCKTYPE type, void *detail);
    int leaveBlock(MD_BLOCKTYPE type, void *detail);
    int enterSpan(MD_SPANTYPE type, void *detail);
    int leaveSpan(MD_SPANTYPE type, void *detail);
    int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size);

    QList<Block> m_blocks;
    QList<Block *> m_blockStack;
    bool m_bold = false, m_italic = false, m_strikethrough = false;
    bool m_code = false, m_math = false, m_mathDisplay = false;
    QString m_linkHref, m_wikiTarget;
};

} // namespace Markoff

#endif // MARKOFF_DOCUMENTBUILDER_P_H
