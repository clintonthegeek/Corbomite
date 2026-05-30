// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/SubpathExtract.h"

#include <QRegularExpression>
#include <QStringList>

namespace Corbomite {

QString extractMarkdownSubpath(const QString &markdown, const QString &subpath)
{
    if (subpath.isEmpty())
        return markdown;

    // Block ID: #^block-id
    if (subpath.startsWith(QStringLiteral("#^"))) {
        const QString blockId = subpath.mid(2); // strip "#^"
        const QStringList lines = markdown.split(QLatin1Char('\n'));

        // Find the line containing ^block-id
        int targetLine = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains(QStringLiteral("^") + blockId)) {
                targetLine = i;
                break;
            }
        }

        if (targetLine < 0)
            return {};

        // Expand to surrounding paragraph (contiguous non-empty lines)
        int start = targetLine;
        while (start > 0 && !lines[start - 1].trimmed().isEmpty())
            --start;

        int end = targetLine;
        while (end < lines.size() - 1 && !lines[end + 1].trimmed().isEmpty())
            ++end;

        // Collect paragraph lines, stripping the block ID marker
        QStringList result;
        const QRegularExpression blockIdPattern(
            QStringLiteral(R"(\s*\^)") + QRegularExpression::escape(blockId));
        for (int i = start; i <= end; ++i) {
            QString line = lines[i];
            line.remove(blockIdPattern);
            result.append(line);
        }

        return result.join(QLatin1Char('\n')).trimmed();
    }

    // Heading: #heading-text
    if (subpath.startsWith(QLatin1Char('#'))) {
        const QString headingText = subpath.mid(1).trimmed(); // strip leading "#"
        const QStringList lines = markdown.split(QLatin1Char('\n'));

        static const QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));

        // Find the matching heading
        int startLine = -1;
        int headingLevel = 0;
        for (int i = 0; i < lines.size(); ++i) {
            auto match = headingPattern.match(lines[i]);
            if (match.hasMatch()) {
                if (match.captured(2).trimmed().compare(headingText, Qt::CaseInsensitive) == 0) {
                    startLine = i;
                    headingLevel = match.captured(1).length();
                    break;
                }
            }
        }

        if (startLine < 0)
            return {};

        // Find the end: next heading of equal or higher level, or EOF
        int endLine = lines.size(); // exclusive
        for (int i = startLine + 1; i < lines.size(); ++i) {
            auto match = headingPattern.match(lines[i]);
            if (match.hasMatch()) {
                int level = match.captured(1).length();
                if (level <= headingLevel) {
                    endLine = i;
                    break;
                }
            }
        }

        // Collect lines from startLine to endLine (exclusive)
        QStringList result;
        for (int i = startLine; i < endLine; ++i) {
            result.append(lines[i]);
        }

        // Trim trailing empty lines
        while (!result.isEmpty() && result.last().trimmed().isEmpty())
            result.removeLast();

        return result.join(QLatin1Char('\n'));
    }

    return markdown;
}

} // namespace Corbomite
