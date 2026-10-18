// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MergeFiles.h"

namespace Corbomite::MergeFiles {

bool hasFrontmatter(const QString &content)
{
    return stripFrontmatter(content) != content;
}

QString stripFrontmatter(const QString &content)
{
    if (!content.startsWith(QLatin1String("---")))
        return content;

    // The opening fence must be the entire first line.
    const int firstNl = content.indexOf(QLatin1Char('\n'));
    const QString firstLine =
        (firstNl < 0 ? content : content.left(firstNl)).trimmed();
    if (firstLine != QLatin1String("---"))
        return content;

    int pos = (firstNl < 0) ? content.size() : firstNl + 1;
    while (pos < content.size()) {
        const int lineEnd = content.indexOf(QLatin1Char('\n'), pos);
        const int end = (lineEnd < 0) ? content.size() : lineEnd;
        const QString line = content.mid(pos, end - pos).trimmed();
        if (line == QLatin1String("---") || line == QLatin1String("...")) {
            int bodyStart = (lineEnd < 0) ? content.size() : lineEnd + 1;
            // Drop a single blank line left behind by the closing fence.
            if (bodyStart < content.size()
                && content.at(bodyStart) == QLatin1Char('\n'))
                ++bodyStart;
            return content.mid(bodyStart);
        }
        if (lineEnd < 0)
            break;
        pos = lineEnd + 1;
    }
    return content;  // no closing fence — not a frontmatter block
}

QString compose(const QString &targetContent,
                const QString &sourceBody,
                bool prepend)
{
    if (targetContent.isEmpty())
        return sourceBody;
    if (sourceBody.isEmpty())
        return targetContent;
    const QString separator = QStringLiteral("\n\n");
    return prepend ? sourceBody + separator + targetContent
                   : targetContent + separator + sourceBody;
}

}  // namespace Corbomite::MergeFiles
