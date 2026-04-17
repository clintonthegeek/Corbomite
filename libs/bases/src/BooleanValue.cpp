// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

bool BooleanValue::equals(const Value &other) const
{
    if (auto *b = dynamic_cast<const BooleanValue *>(&other))
        return m_data == b->m_data;
    return false;
}

bool BooleanValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    // JS-shape: BooleanValue == NumberValue coerces bool to 0/1.
    if (auto *n = dynamic_cast<const NumberValue *>(&other))
        return (m_data ? 1.0 : 0.0) == n->data();
    return false;
}

}  // namespace Corbomite::Bases
