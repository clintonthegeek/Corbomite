// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MomentFormatter.h"

#include <QtCore/QChar>
#include <QtCore/QDate>
#include <QtCore/QStringList>
#include <QtCore/QTime>

namespace Corbomite {

namespace {

// Sentinel characters used to mark escape-bracket placeholders during token
// dispatch. These are ASCII control characters unlikely to appear in format
// strings in the wild.
constexpr QChar kSentinelStart{QChar(0x01)};
constexpr QChar kSentinelEnd{QChar(0x02)};

/// EN-only ordinal suffix table for Phase 1. Other locales fall back to a
/// bare number (matches Obsidian's observed inconsistency for non-EN `Do`).
QString ordinalSuffix(int day, const QLocale &locale)
{
    if (locale.language() == QLocale::English) {
        const int lastTwo = day % 100;
        if (lastTwo >= 11 && lastTwo <= 13) {
            return QStringLiteral("th");
        }
        switch (day % 10) {
            case 1: return QStringLiteral("st");
            case 2: return QStringLiteral("nd");
            case 3: return QStringLiteral("rd");
            default: return QStringLiteral("th");
        }
    }
    return QString{};  // Fallback: bare number, no suffix.
}

/// Pass 1 — Walk the format string, extract `[...]` spans into `escapes`,
/// and return the format with each span replaced by a sentinel placeholder
/// of the form `\x01<index>\x02`.
///
/// Nesting: Obsidian's Moment treats `[...]` as the span from `[` to the
/// next `]`. No nested escapes are supported. An unterminated `[` at the
/// end of the string is treated as a literal `[`.
QString extractEscapes(const QString &input, QStringList &escapes)
{
    QString out;
    out.reserve(input.size());

    int i = 0;
    const int n = input.size();
    while (i < n) {
        const QChar ch = input.at(i);
        if (ch == QLatin1Char('[')) {
            const int closeIdx = input.indexOf(QLatin1Char(']'), i + 1);
            if (closeIdx < 0) {
                // Unterminated [ — treat as literal.
                out.append(ch);
                ++i;
                continue;
            }
            const QString contents = input.mid(i + 1, closeIdx - (i + 1));
            const int idx = escapes.size();
            escapes.append(contents);
            out.append(kSentinelStart);
            out.append(QString::number(idx));
            out.append(kSentinelEnd);
            i = closeIdx + 1;
        } else {
            out.append(ch);
            ++i;
        }
    }
    return out;
}

/// Pass 3 — Restore escape sentinels in the post-tokenization output. Scans
/// for `\x01<digits>\x02` sequences and substitutes the corresponding
/// `escapes[idx]` value.
QString restoreEscapes(const QString &input, const QStringList &escapes)
{
    QString out;
    out.reserve(input.size());

    int i = 0;
    const int n = input.size();
    while (i < n) {
        const QChar ch = input.at(i);
        if (ch == kSentinelStart) {
            const int endIdx = input.indexOf(kSentinelEnd, i + 1);
            if (endIdx < 0) {
                out.append(ch);
                ++i;
                continue;
            }
            const QString numStr = input.mid(i + 1, endIdx - (i + 1));
            bool ok = false;
            const int idx = numStr.toInt(&ok);
            if (ok && idx >= 0 && idx < escapes.size()) {
                out.append(escapes.at(idx));
            }
            i = endIdx + 1;
        } else {
            out.append(ch);
            ++i;
        }
    }
    return out;
}

/// Attempt to match a Moment token starting at `pos` in `src`. On success,
/// appends the formatted value to `out`, sets `consumed` to the token
/// length, and returns true. On failure, returns false and leaves `out`
/// and `consumed` untouched.
bool dispatchToken(const QString &src,
                   int pos,
                   const QDateTime &dt,
                   const QLocale &locale,
                   QString &out,
                   int &consumed)
{
    const int remaining = src.size() - pos;
    auto matches = [&](const char *tok) -> bool {
        const int len = int(qstrlen(tok));
        if (remaining < len) return false;
        for (int k = 0; k < len; ++k) {
            if (src.at(pos + k) != QLatin1Char(tok[k])) return false;
        }
        return true;
    };

    // Longest-match-first. 4-char tokens.
    if (matches("YYYY")) {
        out.append(locale.toString(dt, QStringLiteral("yyyy")));
        consumed = 4; return true;
    }
    if (matches("MMMM")) {
        out.append(locale.toString(dt, QStringLiteral("MMMM")));
        consumed = 4; return true;
    }
    if (matches("DDDD")) {
        // Day-of-year padded to 3 digits (e.g. "015" for Jan 15).
        out.append(QString::number(dt.date().dayOfYear()).rightJustified(3, QLatin1Char('0')));
        consumed = 4; return true;
    }
    if (matches("dddd")) {
        out.append(locale.toString(dt, QStringLiteral("dddd")));
        consumed = 4; return true;
    }
    // 3-char tokens.
    if (matches("MMM")) {
        out.append(locale.toString(dt, QStringLiteral("MMM")));
        consumed = 3; return true;
    }
    if (matches("DDD")) {
        out.append(QString::number(dt.date().dayOfYear()));
        consumed = 3; return true;
    }
    if (matches("ddd")) {
        out.append(locale.toString(dt, QStringLiteral("ddd")));
        consumed = 3; return true;
    }
    if (matches("SSS")) {
        out.append(locale.toString(dt, QStringLiteral("zzz")));
        consumed = 3; return true;
    }

    // 2-char tokens.
    if (matches("YY")) {
        out.append(locale.toString(dt, QStringLiteral("yy")));
        consumed = 2; return true;
    }
    if (matches("MM")) {
        out.append(locale.toString(dt, QStringLiteral("MM")));
        consumed = 2; return true;
    }
    if (matches("DD")) {
        out.append(locale.toString(dt, QStringLiteral("dd")));
        consumed = 2; return true;
    }
    if (matches("Do")) {
        const int day = dt.date().day();
        out.append(QString::number(day));
        out.append(ordinalSuffix(day, locale));
        consumed = 2; return true;
    }
    if (matches("dd")) {
        // 2-char day name. Best-effort: first 2 chars of the locale's
        // short day name. Locale-dependent (e.g. Wed → "We").
        out.append(locale.toString(dt, QStringLiteral("ddd")).left(2));
        consumed = 2; return true;
    }
    if (matches("ww")) {
        int weekYear = 0;
        const int week = dt.date().weekNumber(&weekYear);
        out.append(QString::number(week).rightJustified(2, QLatin1Char('0')));
        consumed = 2; return true;
    }
    if (matches("HH")) {
        out.append(locale.toString(dt, QStringLiteral("HH")));
        consumed = 2; return true;
    }
    if (matches("hh")) {
        // 12-hour with leading zero. Qt's "hh" is 24-hour unless an AP/ap
        // token is present, so compute manually.
        const int h24 = dt.time().hour();
        int h12 = h24 % 12;
        if (h12 == 0) h12 = 12;
        out.append(QString::number(h12).rightJustified(2, QLatin1Char('0')));
        consumed = 2; return true;
    }
    if (matches("mm")) {
        out.append(locale.toString(dt, QStringLiteral("mm")));
        consumed = 2; return true;
    }
    if (matches("ss")) {
        out.append(locale.toString(dt, QStringLiteral("ss")));
        consumed = 2; return true;
    }
    if (matches("SS")) {
        out.append(QString::number(dt.time().msec() / 10).rightJustified(2, QLatin1Char('0')));
        consumed = 2; return true;
    }

    // 1-char tokens. We don't have `Y` (Moment: year with sign) — skip.
    if (matches("M")) {
        out.append(locale.toString(dt, QStringLiteral("M")));
        consumed = 1; return true;
    }
    if (matches("D")) {
        out.append(locale.toString(dt, QStringLiteral("d")));
        consumed = 1; return true;
    }
    if (matches("d")) {
        // Moment: Sun=0, Sat=6. Qt: Mon=1..Sun=7. Convert via modulo 7.
        out.append(QString::number(dt.date().dayOfWeek() % 7));
        consumed = 1; return true;
    }
    if (matches("w")) {
        int weekYear = 0;
        const int week = dt.date().weekNumber(&weekYear);
        out.append(QString::number(week));
        consumed = 1; return true;
    }
    if (matches("H")) {
        out.append(locale.toString(dt, QStringLiteral("H")));
        consumed = 1; return true;
    }
    if (matches("h")) {
        // 12-hour without leading zero.
        const int h24 = dt.time().hour();
        int h12 = h24 % 12;
        if (h12 == 0) h12 = 12;
        out.append(QString::number(h12));
        consumed = 1; return true;
    }
    if (matches("m")) {
        out.append(locale.toString(dt, QStringLiteral("m")));
        consumed = 1; return true;
    }
    if (matches("s")) {
        out.append(locale.toString(dt, QStringLiteral("s")));
        consumed = 1; return true;
    }
    if (matches("S")) {
        out.append(QString::number(dt.time().msec() / 100));
        consumed = 1; return true;
    }
    if (matches("a")) {
        out.append(locale.toString(dt, QStringLiteral("ap")));
        consumed = 1; return true;
    }
    if (matches("A")) {
        out.append(locale.toString(dt, QStringLiteral("AP")));
        consumed = 1; return true;
    }
    if (matches("X")) {
        out.append(QString::number(dt.toSecsSinceEpoch()));
        consumed = 1; return true;
    }
    if (matches("x")) {
        out.append(QString::number(dt.toMSecsSinceEpoch()));
        consumed = 1; return true;
    }

    return false;
}

}  // namespace

QString MomentFormatter::format(const QDateTime &dt,
                                const QString &momentFormat,
                                const QLocale &locale)
{
    if (momentFormat.isEmpty()) {
        return QString{};
    }

    // Pass 1: extract [escape] brackets, replace with sentinel placeholders.
    QStringList escapes;
    const QString sentinelized = extractEscapes(momentFormat, escapes);

    // Pass 2: walk left-to-right, longest-match-first token dispatch.
    QString tokenized;
    tokenized.reserve(sentinelized.size() * 2);

    int i = 0;
    const int n = sentinelized.size();
    while (i < n) {
        const QChar ch = sentinelized.at(i);

        // Preserve sentinel placeholders untouched for Pass 3.
        if (ch == kSentinelStart) {
            const int endIdx = sentinelized.indexOf(kSentinelEnd, i + 1);
            if (endIdx >= 0) {
                tokenized.append(sentinelized.mid(i, endIdx - i + 1));
                i = endIdx + 1;
                continue;
            }
        }

        int consumed = 0;
        if (dispatchToken(sentinelized, i, dt, locale, tokenized, consumed)) {
            i += consumed;
        } else {
            // No token matched — copy char verbatim and advance by 1.
            tokenized.append(ch);
            ++i;
        }
    }

    // Pass 3: restore escape-bracket contents.
    return restoreEscapes(tokenized, escapes);
}

}  // namespace Corbomite
