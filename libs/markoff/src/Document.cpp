// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Document.h"
#include "DocumentBuilder_p.h"

#include <QStringList>
#include <QRegularExpression>

namespace Markoff {

struct Document::Private {
    QString source;
    QString frontmatter;    // YAML frontmatter content (without --- delimiters)
    QList<Block> blocks;
};

Document::Document()
    : d(std::make_unique<Private>())
{
}

Document::~Document() = default;

std::unique_ptr<Document> Document::fromMarkdown(const QString &source)
{
    auto doc = std::unique_ptr<Document>(new Document());
    doc->d->source = source;

    // Extract frontmatter before parsing
    QString markdown = source;
    if (source.startsWith(QStringLiteral("---\n")) || source.startsWith(QStringLiteral("---\r\n"))) {
        int endPos = source.indexOf(QStringLiteral("\n---"), 3);
        if (endPos >= 0) {
            int fmStart = source.indexOf(QLatin1Char('\n')) + 1;
            doc->d->frontmatter = source.mid(fmStart, endPos - fmStart);
            // Skip past the closing ---\n
            int afterFm = endPos + 4; // "\n---"
            if (afterFm < source.size() && source[afterFm] == QLatin1Char('\n'))
                ++afterFm;
            markdown = source.mid(afterFm);
        }
    }

    DocumentBuilder builder;
    if (builder.parse(markdown)) {
        doc->d->blocks = builder.takeBlocks();
        DocumentBuilder::postProcess(doc->d->blocks);
    }

    return doc;
}

QString Document::sourceText() const
{
    return d->source;
}

bool Document::isEmpty() const
{
    return d->source.isEmpty();
}

QString Document::frontmatter() const
{
    return d->frontmatter;
}

QString Document::markdownContent() const
{
    // Return source with frontmatter stripped
    if (d->frontmatter.isEmpty())
        return d->source;

    // Find the end of frontmatter and return everything after
    int endPos = d->source.indexOf(QStringLiteral("\n---"), 3);
    if (endPos < 0)
        return d->source;
    int afterFm = endPos + 4;
    if (afterFm < d->source.size() && d->source[afterFm] == QLatin1Char('\n'))
        ++afterFm;
    return d->source.mid(afterFm);
}

// ---------------------------------------------------------------------------
// extractSubpath
//
// Handles two formats:
//   "#^block-id"  — paragraph containing ^block-id marker
//   "#heading"    — section from matching heading to next same/higher heading
// ---------------------------------------------------------------------------

QString Document::extractSubpath(const QString &subpath) const
{
    if (subpath.isEmpty() || !subpath.startsWith(QLatin1Char('#')))
        return {};

    const QString fragment = subpath.mid(1); // strip leading '#'

    const QStringList lines = d->source.split(QLatin1Char('\n'));

    // -----------------------------------------------------------------------
    // Block-id mode: fragment starts with '^'
    // -----------------------------------------------------------------------
    if (fragment.startsWith(QLatin1Char('^'))) {
        const QString marker = fragment; // e.g. "^myblock"

        // Find the line containing the marker
        int markerLine = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains(marker)) {
                markerLine = i;
                break;
            }
        }
        if (markerLine < 0)
            return {};

        // Expand to contiguous non-empty paragraph (walk up and down)
        int start = markerLine;
        while (start > 0 && !lines[start - 1].trimmed().isEmpty())
            --start;

        int end = markerLine;
        while (end + 1 < lines.size() && !lines[end + 1].trimmed().isEmpty())
            ++end;

        // Collect lines, strip the marker token from whichever line it's on
        QStringList result;
        for (int i = start; i <= end; ++i) {
            QString line = lines[i];
            // Remove the marker (e.g. " ^myblock" including any leading space)
            line.remove(QRegularExpression(QStringLiteral("\\s*\\^[A-Za-z0-9_-]+")));
            result.append(line);
        }

        return result.join(QLatin1Char('\n')).trimmed();
    }

    // -----------------------------------------------------------------------
    // Heading mode
    // -----------------------------------------------------------------------
    // Normalise the fragment: hyphens → spaces, lowercase
    QString needle = fragment;
    needle.replace(QLatin1Char('-'), QLatin1Char(' '));
    needle = needle.toLower().trimmed();

    // Find the heading line whose text matches the needle
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.*)$"));

    int headingLine = -1;
    int headingLevel = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const QRegularExpressionMatch m = headingRe.match(lines[i]);
        if (!m.hasMatch())
            continue;

        QString headText = m.captured(2).toLower().trimmed();
        // Strip any trailing block-id marker (e.g. "My Heading ^abc")
        headText.remove(QRegularExpression(QStringLiteral("\\s*\\^[A-Za-z0-9_-]+$")));

        if (headText == needle) {
            headingLine = i;
            headingLevel = static_cast<int>(m.captured(1).size());
            break;
        }
    }

    if (headingLine < 0)
        return {};

    // Collect lines from the heading line until the next heading of same or
    // higher level (lower or equal '#' count), or EOF
    QStringList result;
    result.append(lines[headingLine]);

    for (int i = headingLine + 1; i < lines.size(); ++i) {
        const QRegularExpressionMatch m = headingRe.match(lines[i]);
        if (m.hasMatch()) {
            const int lvl = static_cast<int>(m.captured(1).size());
            if (lvl <= headingLevel)
                break;
        }
        result.append(lines[i]);
    }

    // Trim trailing blank lines
    while (!result.isEmpty() && result.last().trimmed().isEmpty())
        result.removeLast();

    return result.join(QLatin1Char('\n'));
}

} // namespace Markoff
