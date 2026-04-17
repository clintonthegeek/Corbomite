// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QString>
#include <QStringList>

namespace Corbomite::Bases {

/// Abstract base for every typed cell value in a Bases table.
///
/// Subclasses override `type()` (discriminator used by `isType(name)`
/// and the per-type function registry), `isTruthy()`, `toString()`,
/// `equals()`, `looseEquals()`, `objectAccess()`, `keys()`.
///
/// `equals()` is type-strict structural equality. `looseEquals()` is
/// cross-type coerced (e.g. "2024-01-01"/String == 2024-01-01/Date).
/// Runtime identity is via `std::shared_ptr<Value>` (ValuePtr) — never
/// copy a Value by value, never store Value by value in a container.
class Value
{
public:
    virtual ~Value() = default;

    /// Static-like type discriminator. Values: "Null", "Boolean",
    /// "Number", "String", "List", "Object", "Date", "Duration",
    /// "Regex", "File", "Link", "URL", "Tag", "Icon", "Image",
    /// "HTML", "Markdown", "Error", "ThisFile".
    virtual QString type() const = 0;

    /// Truthiness. Abstract — subclasses define type-specific truth.
    virtual bool isTruthy() const = 0;

    /// Per-type emptiness (used by the `.isEmpty()` function). Default:
    /// the inverse of `isTruthy()`.
    virtual bool isEmpty() const { return !isTruthy(); }

    /// String rendering — also used by `+` concat coercion.
    virtual QString toString() const { return {}; }

    /// Type-strict structural equality. Default: class + toString() match.
    virtual bool equals(const Value &other) const;

    /// Cross-type coerced equality. Default: delegate to equals().
    /// Override to specialise (DateValue / DurationValue / LinkValue /
    /// FileValue coerce specific other-types through parseFromString).
    virtual bool looseEquals(const Value &other) const;

    /// Identifier-style property lookup (`obj.key`). Default: null.
    virtual ValuePtr objectAccess(const QString &key) const;

    /// Keys exposed for auto-complete. Default: empty.
    virtual QStringList keys() const { return {}; }

    /// Null-safe static helpers. `staticEquals(null, null) == true`.
    /// `staticLooseEquals(a, b)` tries `a->looseEquals(b)` then
    /// `b->looseEquals(a)` (symmetric per audit invariant §8).
    static bool staticEquals(const Value *a, const Value *b);
    static bool staticLooseEquals(const Value *a, const Value *b);

    static bool staticEquals(const ValuePtr &a, const ValuePtr &b)
    {
        return staticEquals(a.get(), b.get());
    }
    static bool staticLooseEquals(const ValuePtr &a, const ValuePtr &b)
    {
        return staticLooseEquals(a.get(), b.get());
    }
};

}  // namespace Corbomite::Bases
