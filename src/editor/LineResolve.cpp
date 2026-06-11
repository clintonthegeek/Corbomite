// SPDX-License-Identifier: GPL-3.0-or-later
#include "LineResolve.h"

namespace Corbomite::LineResolve {

// Mirrors the normative flat-line walk in markoff-live's
// EditorWidget.cpp (toCursorPos/fromCursorPos) — keep in lockstep with
// contract-v2 spec §3 if that ever changes.
std::optional<ResolvedLine> resolveLine(const Markoff::MarkoffDocument *doc, int line)
{
    if (!doc || line < 1) return std::nullopt;
    const auto ids = doc->iterateBlocks();
    int cur = 1;
    for (int row = 0; row < int(ids.size()); ++row) {
        const QString text = QString::fromUtf8(doc->blockText(ids[size_t(row)]));
        const int span = 1 + int(text.count(QLatin1Char('\n')));
        if (line < cur + span) {
            int pos = 0;
            for (int i = 0; i < line - cur; ++i)
                pos = int(text.indexOf(QLatin1Char('\n'), pos)) + 1;
            const qsizetype nl = text.indexOf(QLatin1Char('\n'), pos);
            ResolvedLine out;
            out.blockId = ids[size_t(row)];
            out.blockRow = row;
            out.lineStartCharInBlock = pos;
            out.lineText = (nl < 0) ? text.mid(pos) : text.mid(pos, int(nl) - pos);
            return out;
        }
        cur += span;
    }
    return std::nullopt;
}

uint32_t byteOffsetForChar(const QString &blockText, int charPos)
{
    const int clamped = qBound(0, charPos, int(blockText.length()));
    return uint32_t(QStringView(blockText).left(clamped).toUtf8().size());
}

} // namespace Corbomite::LineResolve
