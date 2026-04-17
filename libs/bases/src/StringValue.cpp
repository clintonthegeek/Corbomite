// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

bool StringValue::equals(const Value &other) const
{
    // Strict structural: only same-class (or subclass with same data).
    // Tag/Link/Url/Image/Icon/HTML/Markdown subclass StringValue but have
    // distinct `type()`; keep cross-type-distinct in `equals` (the loose
    // equality operator uses this chain differently).
    if (auto *s = dynamic_cast<const StringValue *>(&other)) {
        // Equal only when the exact type() matches — a Tag and a plain
        // String with the same text are not `equals`.
        return type() == other.type() && m_data == s->m_data;
    }
    return false;
}

ValuePtr StringValue::objectAccess(const QString &key) const
{
    if (key == QLatin1String("length"))
        return std::make_shared<NumberValue>(static_cast<double>(m_data.size()));
    return nullptr;
}

QStringList StringValue::keys() const
{
    return {QStringLiteral("length")};
}

}  // namespace Corbomite::Bases
