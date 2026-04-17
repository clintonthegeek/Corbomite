// SPDX-License-Identifier: GPL-3.0-or-later
//
// Implementations of the StringValue subclasses: Tag/Link/Url/Icon/Image/
// HTML/Markdown. FormulaErrorValue is header-only (all members inline).
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

// ----- TagValue -----

bool TagValue::tagMatches(const QString &other) const
{
    if (m_data == other) return true;
    // Hierarchical: other + "/" is a prefix of m_data (tag under a parent)
    // OR m_data + "/" is a prefix of other (parent of a tag).
    if (m_data.startsWith(other + QLatin1Char('/'))) return true;
    if (other.startsWith(m_data + QLatin1Char('/'))) return true;
    return false;
}

// ----- LinkValue -----

QString LinkValue::toString() const
{
    if (m_display.isEmpty())
        return QStringLiteral("[[%1]]").arg(m_data);
    return QStringLiteral("[[%1|%2]]").arg(m_data, m_display);
}

bool LinkValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    // Coerce a string in the form `[[...]]` to a LinkValue and retry.
    if (auto *s = dynamic_cast<const StringValue *>(&other)) {
        if (auto coerced = parseFromString(s->data()))
            return equals(*coerced);
    }
    return false;
}

std::shared_ptr<LinkValue> LinkValue::parseFromString(const QString &text,
                                                      const QString &sourcePath)
{
    const QString t = text.trimmed();
    if (!t.startsWith(QLatin1String("[[")) || !t.endsWith(QLatin1String("]]")))
        return nullptr;
    const QString inner = t.mid(2, t.size() - 4);
    const int pipe = inner.indexOf(QLatin1Char('|'));
    if (pipe < 0)
        return std::make_shared<LinkValue>(inner, sourcePath);
    return std::make_shared<LinkValue>(
        inner.left(pipe), sourcePath, inner.mid(pipe + 1));
}

}  // namespace Corbomite::Bases
