// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/LinkUtils.h"

#include <QRegularExpression>

namespace Corbomite {

namespace {

// Matches Obsidian's AT regex:
//   / [!"#$%&()*+,.:;<=>?@^`{|}~\/\[\]\\\r\n] / g
QRegularExpression atRegex()
{
    static const QRegularExpression re(
        QStringLiteral("[!\"#$%&()*+,.:;<=>?@^`{|}~/\\[\\]\\\\\\r\\n]"));
    return re;
}

// Matches Obsidian's PT regex:
//   / ([:#|^\\\r\n] | %% | \[\[ | \]\]) / g
QRegularExpression ptRegex()
{
    static const QRegularExpression re(
        QStringLiteral("([:#|^\\\\\\r\\n]|%%|\\[\\[|\\]\\])"));
    return re;
}

QString collapseAndTrim(QString s)
{
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    s.replace(ws, QStringLiteral(" "));
    return s.trimmed();
}

// Is a line a markdown ATX heading? Returns level (1-6) and the raw heading
// text on success; 0 and empty on failure.
int parseHeadingLine(const QString &line, QString *textOut)
{
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.*)$"));
    const auto m = headingRe.match(line);
    if (!m.hasMatch()) return 0;
    if (textOut) *textOut = m.captured(2).trimmed();
    return m.captured(1).length();
}

// Compute the byte offset of the start of `lineIdx` (0-based) in `source`,
// using precomputed line offsets.
int offsetOfLine(const QList<int> &lineStarts, int lineIdx)
{
    if (lineIdx < 0 || lineIdx >= lineStarts.size()) return -1;
    return lineStarts[lineIdx];
}

QList<int> computeLineStarts(const QString &source)
{
    QList<int> offsets;
    offsets.reserve(source.size() / 40 + 1);
    offsets.append(0);
    for (int i = 0; i < source.size(); ++i) {
        if (source[i] == QLatin1Char('\n')) {
            offsets.append(i + 1);
        }
    }
    return offsets;
}

} // namespace

QString stripHeading(const QString &heading)
{
    QString s = heading;
    s.replace(atRegex(), QStringLiteral(" "));
    return collapseAndTrim(s);
}

QString stripHeadingForLink(const QString &heading)
{
    QString s = heading;
    s.replace(ptRegex(), QStringLiteral(" "));
    return collapseAndTrim(s);
}

SubpathResolution resolveSubpath(const Markoff::Document &doc,
                                 const QString &source,
                                 const QString &subpath)
{
    if (subpath.isEmpty() || !subpath.startsWith(QLatin1Char('#'))) {
        return {};
    }

    const QString fragment = subpath.mid(1); // strip leading '#'
    if (fragment.isEmpty()) return {};

    // ---- Footnote: "[^id]" ----------------------------------------------
    if (fragment.startsWith(QStringLiteral("[^")) && fragment.endsWith(QLatin1Char(']'))) {
        const QString id = fragment.mid(2, fragment.size() - 3);
        if (id.isEmpty()) return {};

        // Locate the footnote definition line: "[^id]: ..."
        const QString needle = QStringLiteral("[^") + id + QStringLiteral("]:");
        const int idx = source.indexOf(needle);
        if (idx < 0) return {};

        // Footnote range = from the definition start to end of its paragraph
        // (next blank line) or EOF.
        int end = source.indexOf(QStringLiteral("\n\n"), idx);
        SubpathResolution r;
        r.kind = SubpathResolution::Kind::Footnote;
        r.startOffset = idx;
        r.endOffset = (end < 0) ? -1 : end;
        return r;
    }

    // ---- Block: "^blockid" ----------------------------------------------
    if (fragment.startsWith(QLatin1Char('^'))) {
        const QString marker = fragment; // e.g. "^myblock"
        // Look for marker at end of a paragraph line, preceded by whitespace
        // or at line start. Obsidian matches on a word-boundary basis.
        static const QRegularExpression markerReTemplate(
            QStringLiteral("(?:^|\\s)(\\^[A-Za-z0-9_-]+)"));
        const int needleStart = source.indexOf(marker);
        if (needleStart < 0) return {};

        const QList<int> lineStarts = computeLineStarts(source);

        // Find the line containing the marker
        int markerLine = 0;
        for (int i = 0; i < lineStarts.size(); ++i) {
            const int next = (i + 1 < lineStarts.size()) ? lineStarts[i + 1]
                                                         : source.size() + 1;
            if (needleStart >= lineStarts[i] && needleStart < next) {
                markerLine = i;
                break;
            }
        }

        // Expand to contiguous non-empty paragraph
        const QStringList lines = source.split(QLatin1Char('\n'));
        int start = markerLine;
        while (start > 0 && !lines.value(start - 1).trimmed().isEmpty())
            --start;
        int end = markerLine;
        while (end + 1 < lines.size() && !lines.value(end + 1).trimmed().isEmpty())
            ++end;

        SubpathResolution r;
        r.kind = SubpathResolution::Kind::Block;
        r.startOffset = offsetOfLine(lineStarts, start);
        if (end + 1 < lineStarts.size()) {
            r.endOffset = lineStarts[end + 1] - 1; // exclude trailing newline
        } else {
            r.endOffset = -1;
        }
        return r;
    }

    // ---- Heading --------------------------------------------------------
    const QList<Markoff::HeadingInfo> headings = doc.headings();
    if (headings.isEmpty()) return {};

    const QList<int> lineStarts = computeLineStarts(source);
    const QStringList lines = source.split(QLatin1Char('\n'));

    const QString needle = stripHeading(fragment).toLower();
    if (needle.isEmpty()) return {};

    // Walk headings depth-first, accepting the first whose stripHeading().lower()
    // matches the needle. (Obsidian dispatches segment-by-segment on "/"-joined
    // paths; we match the trailing segment.)
    for (int i = 0; i < headings.size(); ++i) {
        const auto &h = headings[i];
        if (stripHeading(h.text).toLower() != needle) continue;

        SubpathResolution r;
        r.kind = SubpathResolution::Kind::Heading;
        r.headingIndex = i;
        r.startOffset = h.sourceOffset;

        // End = start of the next heading at same-or-higher level, or EOF
        // (signalled as -1).
        int endOffset = -1;
        for (int j = i + 1; j < headings.size(); ++j) {
            if (headings[j].level <= h.level) {
                endOffset = headings[j].sourceOffset;
                break;
            }
        }
        r.endOffset = endOffset;
        return r;
    }

    // Silence unused-var warnings in case lines/lineStarts are not needed
    // on this branch.
    Q_UNUSED(lines);
    Q_UNUSED(lineStarts);

    return {};
}

} // namespace Corbomite
