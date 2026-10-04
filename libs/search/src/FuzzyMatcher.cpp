// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/FuzzyMatcher.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace Corbomite::FuzzyMatcher {

namespace {

// `Yy` — app/own/0779-Yy.js:2. ASCII + general-punctuation; flushes the current
// word-token and emits as a singleton. Excludes whitespace (`Xy`) and CJK
// codepoints (`Qy`).
bool isPunctuation(QChar c)
{
    const ushort u = c.unicode();
    // \u2000-\u206F (general punctuation), \u2E00-\u2E7F (supplemental punctuation).
    if ((u >= 0x2000 && u <= 0x206F) || (u >= 0x2E00 && u <= 0x2E7F)) {
        return true;
    }
    switch (u) {
    case '\\': case '\'': case '!': case '"': case '#': case '$': case '%':
    case '&':  case '(':  case ')': case '*': case '+': case ',': case '-':
    case '.':  case '/':  case ':': case ';': case '<': case '=': case '>':
    case '?':  case '@':  case '[': case ']': case '^': case '_': case '`':
    case '{':  case '|':  case '}': case '~':
        return true;
    default:
        return false;
    }
}

// `Qy` — app/own/0779-Yy.js:4. Tibetan + kana + CJK Hanzi + half-width
// katakana. Each codepoint matches as its own singleton token.
bool isCJKLike(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 0x0F00 && u <= 0x0FFF)
        || (u >= 0x3040 && u <= 0x30FF)
        || (u >= 0x3400 && u <= 0x4DBF)
        || (u >= 0x4E00 && u <= 0x9FFF)
        || (u >= 0xF900 && u <= 0xFAFF)
        || (u >= 0xFF66 && u <= 0xFF9F);
}

// `Xy` = /\s/ — app/own/0779-Yy.js:3. Spelled out rather than QChar::isSpace
// because Qt's set is wider (it adds U+001C-U+001F and U+0085), and the
// boundary predicate below is a scoring rule, not a formatting nicety.
bool isWhitespace(QChar c)
{
    const ushort u = c.unicode();
    switch (u) {
    case 0x0009: case 0x000A: case 0x000B: case 0x000C: case 0x000D:
    case 0x0020: case 0x00A0: case 0x1680: case 0x2028: case 0x2029:
    case 0x202F: case 0x205F: case 0x3000: case 0xFEFF:
        return true;
    default:
        return u >= 0x2000 && u <= 0x200A;
    }
}

// The case-fidelity predicate, verbatim from app/own/0783-eb.js:11. `h` is the
// matched character and `p` the one before it, both taken from the
// ORIGINAL-case candidate so the camelCase clause is detectable. True means the
// landing is mid-word and owes a penalty (non-strict) or must be re-anchored at
// the cursor (strict).
//
//   clause A: h.toLower() !== h  AND  p.toLower() !== p
//   clause B: h.toUpper() !== h  AND  p is not whitespace/punctuation/CJK
//
// Both halves of each clause are load-bearing. Clause A needs h AND p cased-upper,
// i.e. the hit sits inside an all-caps run, so a digit or symbol before an
// uppercase letter is a clean landing. Clause B needs h to have a distinct
// uppercase form at all, so an uncased h (digit, symbol) is never mid-word.
bool isMidWordLanding(QChar h, QChar p)
{
    const QString hs(h);
    const bool hCasedUpper = hs.toLower() != hs;
    const bool hCasedLower = hs.toUpper() != hs;
    const bool pCasedUpper = QString(p).toLower() != p;
    return (hCasedUpper && pCasedUpper)
        || (hCasedLower && !isWhitespace(p) && !isPunctuation(p) && !isCJKLike(p));
}

struct RawMatch {
    QVector<QPair<int, int>> ranges;  // [start, end) over the candidate
    int penalty = 0;                   // clause-A/B landings in word-token mode
};

// `Gy` — app/own/0778-Gy.js:2-10. Sort the ranges, then coalesce overlapping or
// touching ones, leaving a strict gap between survivors.
QVector<QPair<int, int>> gy(QVector<QPair<int, int>> ranges)
{
    if (ranges.isEmpty()) return ranges;
    std::stable_sort(ranges.begin(), ranges.end(),
                     [](const QPair<int, int> &a, const QPair<int, int> &b) {
                         return a.first != b.first ? a.first < b.first : a.second < b.second;
                     });
    QVector<QPair<int, int>> merged;
    merged.reserve(ranges.size());
    merged.append(ranges.first());
    for (int i = 1; i < ranges.size(); ++i) {
        auto &back = merged.last();
        const auto &cur = ranges.at(i);
        if (back.second < cur.first) {
            merged.append(cur);
        } else if (back.second < cur.second) {
            back.second = cur.second;
        }
    }
    return merged;
}

// `eb` — app/own/0783-eb.js:2-30. The two-mode token matcher behind both
// `fuzzySearch` passes.
//
// `strict` is `i`: false for the word-token pass (a mid-word landing is accepted
// at the first occurrence and charged a penalty), true for the char-fuzzy pass
// (a mid-word landing is refused unless it sits exactly on the cursor, and the
// cursor slides forward one token length until it does — never charged).
std::optional<RawMatch> eb(const QStringList &tokens,
                           const QString &candidateLower,
                           const QString &candidateOrig,
                           bool strict)
{
    // app/own/0783-eb.js:3 — an empty token list is "no match", not a match with
    // no ranges. This is what makes an all-whitespace query fail both passes.
    if (tokens.isEmpty()) return std::nullopt;

    RawMatch out;
    int cursor = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const QString &token = tokens.at(i);
        if (token.isEmpty()) continue;
        int u = candidateLower.indexOf(token, cursor);
        if (u < 0) return std::nullopt;

        const QChar h = candidateOrig.at(u);
        if (u > 0 && !isPunctuation(h) && !isCJKLike(h)
            && isMidWordLanding(h, candidateOrig.at(u - 1))) {
            if (!strict) {
                out.penalty += 1;
            } else if (u != cursor) {
                cursor += token.length();
                --i;  // the loop's ++i restores it; retry the same token
                continue;
            }
        }

        const int end = u + static_cast<int>(token.length());
        if (out.ranges.isEmpty() || out.ranges.last().second < u) {
            out.ranges.append({u, end});
        } else {
            out.ranges.last().second = end;
        }
        cursor = end;
    }
    return out;
}

// `rb` — app/own/0787-rb.js:2-11. The simple matcher's candidate test: every
// token must occur as a substring. There is NO ordering constraint across tokens
// — each scan restarts at `l = -1` — so "foo bar" matches "barxxfoo". The
// `l += c + 1` cursor only keeps one token's own occurrences from overlapping
// each other. All of them are recorded, because they drive the highlight ranges.
bool rb(const QStringList &tokens,
        const QString &candidateLower,
        QVector<QPair<int, int>> &out)
{
    for (const QString &token : tokens) {
        if (token.isEmpty()) continue;
        bool found = false;
        int pos = -1;
        while ((pos = candidateLower.indexOf(token, pos < 0 ? 0 : pos + token.length() + 1)) >= 0) {
            found = true;
            out.append({pos, pos + static_cast<int>(token.length())});
        }
        if (!found) return false;
    }
    return true;
}

// `Jy` — app/own/0782-Jy.js:2-8. Five-term scorer; lower is better, so callers
// negate nothing and just compare descending.
double jy(const QVector<QPair<int, int>> &matches,
          int queryLen,
          int candidateLen,
          int penalty)
{
    if (matches.isEmpty()) return 0.0;
    double r = 0.0;
    r -= std::max(0, static_cast<int>(matches.size()) - 1);      // one point per gap
    r -= penalty / 10.0;                                          // case fidelity
    const int first = matches.first().first;
    const int last = matches.last().second;
    r -= (last - first + 1 - queryLen) / 100.0;                   // span beyond the query
    r -= first / 1000.0;                                          // start bias
    r -= candidateLen / 10000.0;                                  // length tiebreak
    return r;
}

PreparedQuery tokenise(const QString &query, bool simple)
{
    PreparedQuery prepared;
    prepared.query = query;
    prepared.simple = simple;

    const QString lower = query.toLower();
    QString currentWord;

    auto flushWord = [&]() {
        if (!currentWord.isEmpty()) {
            prepared.tokens.append(currentWord);
            currentWord.clear();
        }
    };

    for (QChar c : lower) {
        // `fuzzy` is the second pass's character alphabet: the whole lowercased
        // query minus literal spaces (app/own/0780-$y.js:10-12). Only the space
        // is dropped — a tab survives into the alphabet. The simple matcher has
        // no second pass and therefore no alphabet.
        if (!simple && c != QLatin1Char(' ')) prepared.fuzzy.append(c);

        if (c.isSpace()) {
            flushWord();
            continue;
        }
        if (simple) {
            // `ab` — app/own/0789-ab.js:3: whitespace-only split.
            currentWord.append(c);
            continue;
        }
        if (isPunctuation(c) || isCJKLike(c)) {
            flushWord();
            prepared.tokens.append(QString(c));
            continue;
        }
        currentWord.append(c);
    }
    flushWord();
    return prepared;
}

} // namespace

PreparedQuery prepareQuery(const QString &query)
{
    return tokenise(query, /*simple=*/false);
}

PreparedQuery prepareSimpleSearch(const QString &query)
{
    return tokenise(query, /*simple=*/true);
}

std::optional<FuzzyMatch> fuzzySearch(const PreparedQuery &query, const QString &haystack)
{
    // Empty query short-circuit (app/own/0784-tb.js:3-6) — every candidate
    // matches with an empty range list. Load-bearing for "show all" on a fresh
    // popup open.
    if (query.query.isEmpty()) {
        return FuzzyMatch{0.0, {}};
    }

    const int queryLen = query.query.length();

    if (query.simple) {
        // `ab` -> `rb` + `ob` (app/own/0789-ab.js:2-7). No char-fuzzy fallback,
        // no punctuation/CJK tokenisation, penalty hardcoded to 0, and the
        // scorer sees the ORIGINAL candidate length — unlike the fuzzy path,
        // which is handed the lowercased one.
        QVector<QPair<int, int>> ranges;
        if (!rb(query.tokens, haystack.toLower(), ranges)) return std::nullopt;
        FuzzyMatch fm;
        fm.matches = gy(std::move(ranges));
        fm.score = jy(fm.matches, queryLen, haystack.length(), 0);
        return fm;
    }

    const QString lower = haystack.toLower();

    // Pass 1 — word-token match, `strict` false (app/own/0784-tb.js:7).
    if (auto pass1 = eb(query.tokens, lower, haystack, /*strict=*/false)) {
        FuzzyMatch fm;
        fm.matches = std::move(pass1->ranges);
        fm.score = jy(fm.matches, queryLen, lower.length(), pass1->penalty);
        return fm;
    }

    // Pass 2 — char-fuzzy fallback, `strict` true (app/own/0784-tb.js:8).
    QStringList chars;
    chars.reserve(query.fuzzy.size());
    for (QChar c : query.fuzzy) chars.append(QString(c));

    if (auto pass2 = eb(chars, lower, haystack, /*strict=*/true)) {
        FuzzyMatch fm;
        fm.matches = std::move(pass2->ranges);
        fm.score = jy(fm.matches, queryLen, lower.length(), pass2->penalty);
        return fm;
    }
    return std::nullopt;
}

void sortSearchResults(QVector<FuzzyMatch> &results)
{
    std::stable_sort(results.begin(), results.end(),
                     [](const FuzzyMatch &a, const FuzzyMatch &b) { return a.score > b.score; });
}

} // namespace Corbomite::FuzzyMatcher
