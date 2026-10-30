// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/IgnoreFilter.h"

#include <QJsonArray>
#include <QJsonObject>

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

IgnoreFilter IgnoreFilter::fromAppJson(const QJsonValue &appJson)
{
    if (!appJson.isObject()) return {};
    const QJsonArray arr = appJson.toObject()
        .value(QStringLiteral("userIgnoreFilters")).toArray();
    QStringList patterns;
    patterns.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (v.isString()) patterns.append(v.toString());
    }
    return fromPatterns(patterns);
}

bool IgnoreFilter::matches(const QString &vaultRelativePath) const
{
    for (const auto &p : m_patterns) {
        if (p.match(vaultRelativePath).hasMatch()) return true;
    }
    return false;
}

} // namespace Corbomite
