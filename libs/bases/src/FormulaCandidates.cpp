// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaCandidates.h"

#include "corbomite/bases/FunctionRegistry.h"

#include <QSet>

namespace Corbomite::Bases::FormulaCandidates {

// Identifier characters per the Bases DSL: [A-Za-z0-9_$].
static bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$');
}

QStringList build(const QVector<PropertyId> &props,
                  const FunctionRegistry *funcs, Mode mode)
{
    QSet<QString> set;
    set.insert(QStringLiteral("this"));
    set.insert(QStringLiteral("note"));
    set.insert(QStringLiteral("file"));
    set.insert(QStringLiteral("formula"));
    if (mode == Mode::SummaryFormula) set.insert(QStringLiteral("values"));

    for (const auto &p : props) {
        if (!p.name.isEmpty()) set.insert(p.name);
        if (p.kind == PropertyKind::Formula)
            set.insert(QStringLiteral("formula.") + p.name);
    }

    if (funcs)
        for (const auto &n : funcs->allNames()) set.insert(n);

    QStringList out(set.constBegin(), set.constEnd());
    out.sort();
    return out;
}

TokenSpan tokenAt(const QString &text, int cursor)
{
    cursor = qBound(0, cursor, int(text.size()));
    int start = cursor;
    while (start > 0 && isIdentChar(text.at(start - 1))) --start;
    return { start, text.mid(start, cursor - start) };
}

}  // namespace Corbomite::Bases::FormulaCandidates
