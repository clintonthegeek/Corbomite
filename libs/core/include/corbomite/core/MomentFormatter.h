// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QString>

namespace Corbomite {

/// Translates Moment.js format strings to QDateTime output.
/// Tokens covered: YYYY YY M MM MMM MMMM D DD Do DDD DDDD d dd ddd dddd
/// w ww H HH h hh m mm s ss S SS SSS a A X x. Unknown tokens pass through
/// verbatim. Escape-bracket `[literal]` preserves contents without token
/// interpretation.
class MomentFormatter {
public:
    static QString format(const QDateTime &dt,
                          const QString &momentFormat,
                          const QLocale &locale = QLocale());
};

}  // namespace Corbomite
