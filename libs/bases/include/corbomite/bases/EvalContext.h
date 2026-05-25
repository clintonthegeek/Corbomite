// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace Corbomite::Bases {

class VaultResolver;

/// Runtime context passed to the evaluator. Concrete implementations:
///   - BasesEntry (Phase 7) — canonical one-row context.
///   - ShadowingContext (Phase 5) — iteration-bound scope for
///     list.map/filter/reduce and object.map/filter bodies.
///   - SummaryContext (Phase 8) — binds `values` to the per-group list.
class EvalContext
{
public:
    virtual ~EvalContext() = default;

    /// Resolve a bare identifier. Returns nullptr (not NullValue) when
    /// the name is unrecognised — the caller distinguishes.
    virtual ValuePtr getByIdentifier(const QString &name) const = 0;

    /// Keys exposed for auto-complete. Not used at evaluation time.
    virtual QStringList keys() const { return {}; }

    /// Vault-access seam for vault-bound builtins. Default: unbound.
    virtual const VaultResolver *vault() const { return nullptr; }
};

/// Lambda-backed adapter. Useful in tests and for simple host-side
/// contexts where building a full BasesEntry is overkill.
class LambdaContext : public EvalContext
{
public:
    using Fn = std::function<ValuePtr(const QString &)>;
    explicit LambdaContext(Fn f) : m_f(std::move(f)) {}
    ValuePtr getByIdentifier(const QString &name) const override { return m_f ? m_f(name) : nullptr; }

private:
    Fn m_f;
};

}  // namespace Corbomite::Bases
