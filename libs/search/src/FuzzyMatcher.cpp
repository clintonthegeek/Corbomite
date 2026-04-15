// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/search/FuzzyMatcher.h"

#include <algorithm>
#include <optional>

namespace Corbomite::FuzzyMatcher {

namespace {

// `ky` from search.md §1 — ASCII + general-punctuation that flushes the current
// word-token and emits as a singleton. Excludes whitespace (handled by `cy`)
// and CJK codepoints (handled by `ey`).
bool isPunctuation(QChar c)
{
    ushort u = c.unicode();
    // \u2000-\u206F (general punctuation), \u2E00-\u2E7F (supplemental punctuation).
    if ((u >= 0x2000 && u <= 0x206F) || (u >= 0x2E00 && u <= 0x2E7F)) {
        return true;
    }
    // ASCII set from the `ky` regex.
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

// `ey` from search.md §1 — Tibetan + Japanese + CJK Hanzi + half-width katakana.
// Each codepoint is matched as its own singleton token (per-codepoint CJK match
// without dictionary or romaji conversion).
bool isCJKLike(QChar c)
{
    ushort u = c.unicode();
    return (u >= 0x0F00 && u <= 0x0FFF)   // Tibetan
        || (u >= 0x3040 && u <= 0x30FF)   // Hiragana + Katakana
        || (u >= 0x3400 && u <= 0x4DBF)   // CJK Unified Ext A
        || (u >= 0x4E00 && u <= 0x9FFF)   // CJK Unified
        || (u >= 0xF900 && u <= 0xFAFF)   // CJK Compat Ideographs
        || (u >= 0xFF66 && u <= 0xFF9F);  // Half-width katakana
}

// A position `i` is on a word boundary (so a match landing there pays no
// case-mismatch penalty) iff: position 0, after whitespace/punct/CJK, or
// at a lowercase→uppercase transition (camelCase). Inspect ORIGINAL-case
// candidate so the camelCase rule is detectable.
bool isWordBoundary(const QString &candidate, int i)
{
    if (i == 0) return true;
    QChar prev = candidate.at(i - 1);
    if (prev.isSpace() || isPunctuation(prev) || isCJKLike(prev)) return true;
    QChar curr = candidate.at(i);
    return prev.isLower() && curr.isUpper();
}

struct RawMatch {
    QVector<QPair<int, int>> ranges;  // [start, end) in candidate, NOT yet merged
    int penalty = 0;                   // count of mid-word landings (word-token mode)
};

// `ty` from _internal.js:83097-83224 — single-pass left-to-right matcher.
// `tokens` is iterated in order; each must appear via indexOf at or after the
// running cursor. `strict` (char-fuzzy mode) prefers a word-boundary landing —
// it scans ahead for a boundary occurrence and only falls back to the first
// (mid-word) hit when no boundary occurrence exists. Word-token mode (strict
// false) accepts the first hit immediately, charging a penalty if mid-word.
std::optional<RawMatch> ty(const QStringList &tokens,
                            const QString &candidateLower,
                            const QString &candidateOrig,
                            bool strict)
{
    RawMatch out;
    int cursor = 0;
    for (const QString &token : tokens) {
        if (token.isEmpty()) continue;
        const int firstHit = candidateLower.indexOf(token, cursor);
        if (firstHit < 0) return std::nullopt;

        int chosen = firstHit;
        bool atBoundary = isWordBoundary(candidateOrig, firstHit);
        if (!atBoundary && strict) {
            int probe = candidateLower.indexOf(token, firstHit + 1);
            while (probe >= 0 && !isWordBoundary(candidateOrig, probe)) {
                probe = candidateLower.indexOf(token, probe + 1);
            }
            if (probe >= 0) {
                chosen = probe;
                atBoundary = true;
            }
        }
        if (!atBoundary) out.penalty += 1;

        out.ranges.append({chosen, chosen + static_cast<int>(token.length())});
        cursor = chosen + token.length();
    }
    return out;
}

// `wy` from _internal.js — coalesce touching ranges into one. Invariant: callers
// can iterate matches assuming strict gaps between consecutive ranges.
QVector<QPair<int, int>> mergeAdjacent(const QVector<QPair<int, int>> &ranges)
{
    if (ranges.isEmpty()) return {};
    QVector<QPair<int, int>> merged;
    merged.reserve(ranges.size());
    merged.append(ranges.first());
    for (int i = 1; i < ranges.size(); ++i) {
        auto &back = merged.last();
        const auto &cur = ranges.at(i);
        if (back.second == cur.first) {
            back.second = cur.second;
        } else {
            merged.append(cur);
        }
    }
    return merged;
}

// `xy` from _internal.js:83133-83143 — five-term hand-tuned scorer.
// Lower (more negative) is better than zero; callers compare with > so we
// negate at the call site to keep "higher = better" externally.
double scoreXy(const QVector<QPair<int, int>> &matches,
               int queryLen,
               int candidateLen,
               int penalty)
{
    if (matches.isEmpty()) return 0.0;
    double r = 0.0;
    r -= std::max(0, static_cast<int>(matches.size()) - 1);                 // contiguity (gaps)
    r -= penalty / 10.0;                                                    // case-fidelity
    const int first = matches.first().first;
    const int last = matches.last().second;
    r -= (last - first + 1 - queryLen) / 100.0;                             // span cost
    r -= first / 1000.0;                                                    // start bias
    r -= candidateLen / 10000.0;                                            // length tiebreak
    return r;
}

PreparedQuery tokenise(const QString &query, bool simple)
{
    PreparedQuery prepared;
    prepared.query = query;
    prepared.simple = simple;

    const QString lower = query.toLower();
    QString currentWord;
    QString fuzzyAccum;
    fuzzyAccum.reserve(lower.size());

    auto flushWord = [&]() {
        if (!currentWord.isEmpty()) {
            prepared.tokens.append(currentWord);
            currentWord.clear();
        }
    };

    for (QChar c : lower) {
        if (c.isSpace()) {
            flushWord();
            continue;
        }
        fuzzyAccum.append(c);
        if (simple) {
            // Whitespace-only tokenisation; no CJK split, no punctuation singletons.
            currentWord.append(c);
            continue;
        }
        if (isPunctuation(c)) {
            flushWord();
            prepared.tokens.append(QString(c));
            continue;
        }
        if (isCJKLike(c)) {
            flushWord();
            prepared.tokens.append(QString(c));
            continue;
        }
        currentWord.append(c);
    }
    flushWord();
    prepared.fuzzy = fuzzyAccum;
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
    // Empty query short-circuit (search.md §1) — every candidate matches with
    // empty range list. Load-bearing for "show all" on fresh popup open.
    if (query.query.isEmpty()) {
        return FuzzyMatch{0.0, {}};
    }

    const QString lower = haystack.toLower();
    const int queryLen = query.query.length();
    const int candLen = haystack.length();

    // Pass 1 — word-token match.
    if (auto pass1 = ty(query.tokens, lower, haystack, /*strict=*/false)) {
        FuzzyMatch fm;
        fm.matches = mergeAdjacent(pass1->ranges);
        fm.score = scoreXy(fm.matches, queryLen, candLen, pass1->penalty);
        return fm;
    }

    // Pass 2 — char-fuzzy fallback (skipped in simple mode).
    if (query.simple) return std::nullopt;

    QStringList chars;
    chars.reserve(query.fuzzy.size());
    for (QChar c : query.fuzzy) chars.append(QString(c));

    if (auto pass2 = ty(chars, lower, haystack, /*strict=*/true)) {
        FuzzyMatch fm;
        fm.matches = mergeAdjacent(pass2->ranges);
        fm.score = scoreXy(fm.matches, queryLen, candLen, pass2->penalty);
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
