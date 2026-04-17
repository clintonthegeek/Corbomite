// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Formula.h"

#include "corbomite/bases/Evaluator.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/Parser.h"
#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

Formula::Formula(QString source) : m_source(std::move(source))
{
    QString err;
    auto expr = Parser::parse(m_source, &err);
    if (auto *inv = dynamic_cast<InvalidExpr *>(expr.get())) {
        m_parseError = inv->message;
    }
    m_ast.reset(expr.release());
}

Formula::Formula(const Formula &other)
    : m_source(other.m_source),
      m_ast(other.m_ast ? std::shared_ptr<Expr>{} : nullptr),
      m_parseError(other.m_parseError)
{
    // Re-parse to get a fresh AST owned by this copy. Cheap.
    if (!m_source.isEmpty()) {
        QString err;
        auto expr = Parser::parse(m_source, &err);
        m_ast.reset(expr.release());
    }
}

Formula &Formula::operator=(const Formula &other)
{
    if (this != &other) {
        m_source = other.m_source;
        m_parseError = other.m_parseError;
        if (!m_source.isEmpty()) {
            QString err;
            auto expr = Parser::parse(m_source, &err);
            m_ast.reset(expr.release());
        } else {
            m_ast.reset();
        }
    }
    return *this;
}

ValuePtr Formula::getValue(const EvalContext &ctx, FunctionRegistry *funcs) const
{
    if (!m_ast) return NullValue::instance();
    Evaluator ev(funcs ? funcs : &FunctionRegistry::global());
    return ev.eval(*m_ast, ctx);
}

bool Formula::test(const EvalContext &ctx, FunctionRegistry *funcs) const
{
    auto v = getValue(ctx, funcs);
    return v && v->isTruthy();
}

}  // namespace Corbomite::Bases
