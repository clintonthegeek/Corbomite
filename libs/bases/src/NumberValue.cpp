// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include <cmath>

namespace Corbomite::Bases {

bool NumberValue::isTruthy() const
{
    return m_data != 0.0 && !std::isnan(m_data);
}

QString NumberValue::toString() const
{
    if (std::isnan(m_data))
        return QStringLiteral("NaN");
    if (std::isinf(m_data))
        return QStringLiteral("∞");
    // Match JS's Number.prototype.toString default: integer-form for integral
    // values under 2^53 (the exactly-representable-as-double integer range).
    if (m_data == std::floor(m_data) && std::fabs(m_data) < 1e15)
        return QString::number(static_cast<qint64>(m_data));
    return QString::number(m_data, 'g', 15);
}

bool NumberValue::equals(const Value &other) const
{
    if (auto *n = dynamic_cast<const NumberValue *>(&other))
        return m_data == n->m_data;
    return false;
}

bool NumberValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    if (auto *b = dynamic_cast<const BooleanValue *>(&other))
        return m_data == (b->data() ? 1.0 : 0.0);
    return false;
}

}  // namespace Corbomite::Bases
