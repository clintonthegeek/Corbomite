// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PropertyId.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Corbomite::Bases {

class FunctionRegistry;

namespace FormulaCandidates {

enum class Mode { NamedFormula, SummaryFormula };

/// Build the flat (non-type-aware) candidate token list:
///   - the identifier roots this/note/file/formula
///   - one token per property (its `name`; formulas also as `formula.<name>`)
///   - every function name from the registry
///   - `values` first, in SummaryFormula mode
/// Result is sorted + deduped.
QStringList build(const QVector<PropertyId> &props,
                  const FunctionRegistry *funcs,
                  Mode mode);

/// The identifier token ending at code-unit offset `cursor` in `text`
/// (clamped to [0, size]): scans left over [A-Za-z0-9_$]. Returns
/// {tokenStart, token}. Empty token when the char before the clamped cursor
/// is not an identifier char.
struct TokenSpan { int start; QString token; };
TokenSpan tokenAt(const QString &text, int cursor);

}  // namespace FormulaCandidates
}  // namespace Corbomite::Bases
