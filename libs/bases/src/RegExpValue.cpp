// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

QString RegExpValue::toString() const
{
    return QStringLiteral("/%1/%2").arg(m_source, m_flags);
}

std::shared_ptr<RegExpValue> RegExpValue::parseFromString(const QString &literal)
{
    const QString t = literal;
    if (!t.startsWith(QLatin1Char('/')) || t.size() < 2) return nullptr;
    // Find the closing `/` — respecting `\/` escape.
    int end = -1;
    for (int i = 1; i < t.size(); ++i) {
        if (t[i] == QLatin1Char('\\') && i + 1 < t.size()) { ++i; continue; }
        if (t[i] == QLatin1Char('/')) { end = i; break; }
    }
    if (end < 0) return nullptr;
    const QString body = t.mid(1, end - 1);
    const QString flags = t.mid(end + 1);

    QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
    if (flags.contains(QLatin1Char('i')))
        opts |= QRegularExpression::CaseInsensitiveOption;
    if (flags.contains(QLatin1Char('m')))
        opts |= QRegularExpression::MultilineOption;
    if (flags.contains(QLatin1Char('s')))
        opts |= QRegularExpression::DotMatchesEverythingOption;

    QRegularExpression re(body, opts);
    if (!re.isValid()) return nullptr;
    return std::make_shared<RegExpValue>(std::move(re), body, flags);
}

}  // namespace Corbomite::Bases
