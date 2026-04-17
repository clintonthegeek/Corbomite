// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

// Accessor struct wraps NullValue's private constructor so we can
// shared_ptr-manage the singleton without making the ctor public.
struct NullValueAccess : NullValue {};

ValuePtr NullValue::instance()
{
    static ValuePtr s_null = std::shared_ptr<Value>(new NullValueAccess());
    return s_null;
}

}  // namespace Corbomite::Bases
