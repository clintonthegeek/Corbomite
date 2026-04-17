// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <functional>
#include <typeindex>

namespace Corbomite::Bases {

class EvalContext;

/// Parameter descriptor (addendum §8.1). `types` is an OR-union of allowed
/// Value-subclass type_indices. Empty list means "any".
struct FnParam
{
    QString name;
    QVector<std::type_index> types;
    bool optional = false;
    bool variadic = false;
};

/// A function callable from formulas.
///
/// For global functions, `apply` receives the already-evaluated argument
/// list. For per-type (instance) functions, the first argument is the
/// evaluated subject (the receiver of `x.method(...)`); the rest are the
/// call-site arguments.
struct BasesFunction
{
    QString name;
    QVector<FnParam> params;
    std::function<ValuePtr(const EvalContext &, const QVector<ValuePtr> &)> apply;
    QString docString;
};

/// Registry used by Evaluator::evalCall.
///
/// Per-type lookup walks the receiver's class chain (most-derived to
/// Value-base) and returns the first match. Global lookup is by
/// lowercased function name.
class FunctionRegistry
{
public:
    void addGlobal(BasesFunction fn);
    void addForType(std::type_index valueClass, BasesFunction fn);

    const BasesFunction *findInstance(const Value *subject, const QString &name) const;
    const BasesFunction *findGlobal(const QString &name) const;

    void removeGlobal(const QString &name);
    void removeForType(std::type_index valueClass, const QString &name);

    /// Shared global registry — built-ins register into this at static-
    /// init time via registerBuiltins().
    static FunctionRegistry &global();

private:
    QHash<QString, BasesFunction> m_global;
    QHash<std::type_index, QHash<QString, BasesFunction>> m_byType;
};

/// Helpers for building FnParams.
FnParam requiredParam(QString name, QVector<std::type_index> types = {});
FnParam optionalParam(QString name, QVector<std::type_index> types = {});
FnParam variadicTail(QString name, QVector<std::type_index> types = {});

/// Register every built-in (globals + per-type methods + default
/// summary formulas). Called exactly once from FunctionRegistry::global()
/// on first access.
void registerBuiltins(FunctionRegistry &r);

}  // namespace Corbomite::Bases
