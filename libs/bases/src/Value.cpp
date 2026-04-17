// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Value.h"

namespace Corbomite::Bases {

bool Value::equals(const Value &other) const
{
    return type() == other.type() && toString() == other.toString();
}

bool Value::looseEquals(const Value &other) const
{
    return equals(other);
}

ValuePtr Value::objectAccess(const QString &) const
{
    return nullptr;
}

bool Value::staticEquals(const Value *a, const Value *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return a->equals(*b);
}

bool Value::staticLooseEquals(const Value *a, const Value *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    return a->looseEquals(*b) || b->looseEquals(*a);
}

}  // namespace Corbomite::Bases
