// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/IgnoreFilter.h"

namespace Corbomite {

IgnoreFilter IgnoreFilter::fromPatterns(const QStringList &patterns)
{
    IgnoreFilter out;
    for (const QString &raw : patterns) {
        const QString entry = raw.trimmed();
        if (entry.isEmpty()) continue;

        QRegularExpression re;
        if (entry.size() > 2 && entry.startsWith(QLatin1Char('/'))
                           && entry.endsWith(QLatin1Char('/'))) {
            re = QRegularExpression(entry.mid(1, entry.size() - 2),
                                    QRegularExpression::CaseInsensitiveOption);
        } else {
            // Obsidian escapes with /[.?*+^$[\]\\(){}|-]/g, which is
            // observationally equivalent to QRegularExpression::escape: an
            // exhaustive ASCII sweep showed both forms match their own
            // literal input for every codepoint, differing only in pattern
            // text.
            re = QRegularExpression(
                QLatin1Char('^') + QRegularExpression::escape(entry),
                QRegularExpression::CaseInsensitiveOption);
        }
        // Obsidian logs "Bad regex for user ignore filter" and continues.
        if (!re.isValid()) continue;
        out.m_patterns.append(std::move(re));
    }
    return out;
}

bool IgnoreFilter::matches(const QString &vaultRelativePath) const
{
    for (const auto &p : m_patterns) {
        if (p.match(vaultRelativePath).hasMatch()) return true;
    }
    return false;
}

} // namespace Corbomite
