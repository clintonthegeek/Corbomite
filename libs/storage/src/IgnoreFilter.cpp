// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/IgnoreFilter.h"

namespace Corbomite {

IgnoreFilter IgnoreFilter::fromPatterns(const QStringList &patterns)
{
    IgnoreFilter out;
    for (const QString &raw : patterns) {
        if (raw.isEmpty()) continue;

        Pattern p;
        p.original = raw;

        // `/…/` form → regex; everything else is a plain prefix.
        if (raw.size() >= 2 && raw.startsWith(QLatin1Char('/'))
                            && raw.endsWith(QLatin1Char('/'))) {
            const QString body = raw.mid(1, raw.size() - 2);
            QRegularExpression re(body);
            if (re.isValid()) {
                p.re = std::move(re);
                p.isRegex = true;
                out.m_patterns.append(std::move(p));
            }
            // Invalid regex: silently skip (matches Obsidian's console-warn + continue).
        } else {
            // Plain prefix — anchor at path start, literal-escape.
            p.re = QRegularExpression(
                QStringLiteral("^") + QRegularExpression::escape(raw));
            p.isRegex = false;
            out.m_patterns.append(std::move(p));
        }
    }
    return out;
}

bool IgnoreFilter::matches(const QString &vaultRelativePath) const
{
    for (const auto &p : m_patterns) {
        if (p.re.match(vaultRelativePath).hasMatch()) return true;
    }
    return false;
}

} // namespace Corbomite
