// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Ast.h"
#include "EvalContext.h"

#include <QString>

#include <memory>
#include <optional>

namespace Corbomite::Bases {

class FunctionRegistry;

/// Top-level formula container (addendum §1 `DK`).
///
/// Holds the original source text + a parsed AST. `getValue()` evaluates
/// against a context; `test()` evaluates and returns isTruthy().
/// Round-trips through `toString()` for YAML serialisation.
///
/// An invalid source string does not throw: it stores an `InvalidExpr`
/// internally; getValue returns FormulaErrorValue and test returns false.
class Formula
{
public:
    Formula() = default;
    explicit Formula(QString source);

    Formula(const Formula &other);
    Formula &operator=(const Formula &other);
    Formula(Formula &&) noexcept = default;
    Formula &operator=(Formula &&) noexcept = default;

    const QString &source() const { return m_source; }
    QString toString() const { return m_source; }

    bool isValid() const { return !m_parseError.has_value(); }
    std::optional<QString> parseError() const { return m_parseError; }

    ValuePtr getValue(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const;
    bool test(const EvalContext &ctx, FunctionRegistry *funcs = nullptr) const;

private:
    QString m_source;
    std::shared_ptr<Expr> m_ast;        // shared so Formula is copy-safe
    std::optional<QString> m_parseError;
};

}  // namespace Corbomite::Bases
