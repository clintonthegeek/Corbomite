// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/EphemeralState.h"

#include <QJsonArray>
#include <QJsonValue>

namespace Corbomite {

namespace {

// Known top-level keys in the eState object. Anything not in this set is
// copied into `extraKeys` verbatim so round-tripping preserves future
// Obsidian fields / plugin data (Cluster B WorkspaceState idiom).
// Corbomite's own scroll key. Obsidian's
// `eState.scroll` is a LINE NUMBER (obsidian-audit/domains/views.md:159), but
// Corbomite's contract-v2 `MarkdownView::scrollPositionVisualLine()` returns a
// 0.0-1.0 viewport FRACTION. Writing the fraction under Obsidian's key made a
// shared vault's scroll position unreadable to Obsidian (it would restore to
// line 0), and made ours unreadable to Obsidian's writer. Per the Cluster L
// compat doctrine — schema-at-rest fidelity to Obsidian is the only contract,
// in-memory shape is free to be idiomatic — we keep our fraction under a
// private key and leave Obsidian's `scroll` alone. The conversion that would
// make them agree needs the viewport height, which is not available at
// serialize time and is not stable across window sizes anyway.
const char *kKeyScrollFraction = "corbomite.scrollFraction";
const char *kKeyCursor         = "cursor";
const char *kKeyCursorLine     = "line";
const char *kKeyCursorColumn   = "column";
const char *kKeyMode           = "mode";
const char *kKeySource         = "source";
const char *kKeyFoldedHeadings = "foldedHeadings";

bool isKnownKey(const QString &key)
{
    return key == QLatin1StringView(kKeyCursor)
        || key == QLatin1StringView(kKeyMode)
        || key == QLatin1StringView(kKeySource)
        || key == QLatin1StringView(kKeyFoldedHeadings);
}

} // namespace

QJsonObject EphemeralState::toJson() const
{
    // Start from extraKeys to preserve unknown fields; typed fields
    // overwrite any same-name keys from extraKeys (shouldn't happen because
    // fromJson only stashes unknown keys, but this is the safer compose).
    QJsonObject obj = extraKeys;

    // Deliberately NOT written under Obsidian's `scroll` key — see
    // kKeyScrollFraction above. If Obsidian wrote a real value for `scroll`, it is
    // already in `extraKeys` and passes through untouched (extraKeys is the
    // base object above).
    obj.insert(QLatin1StringView(kKeyScrollFraction), static_cast<double>(scroll));

    QJsonObject cursorObj;
    cursorObj.insert(QLatin1StringView(kKeyCursorLine), cursor.line);
    cursorObj.insert(QLatin1StringView(kKeyCursorColumn), cursor.column);
    obj.insert(QLatin1StringView(kKeyCursor), cursorObj);

    obj.insert(QLatin1StringView(kKeyMode), modeRaw);

    // `source` field only meaningful when mode == "source". When mode is
    // "preview" we still emit the field as false for determinism; readers
    // must ignore it per the audit. The ViewModeSerializer drops it for
    // preview on the input path; what we emit on output is a contract
    // choice — keeping determinism simplifies golden-fixture testing.
    obj.insert(QLatin1StringView(kKeySource), sourceFlag);

    QJsonArray folds;
    for (int line : foldedHeadings) folds.append(line);
    obj.insert(QLatin1StringView(kKeyFoldedHeadings), folds);

    return obj;
}

EphemeralState EphemeralState::fromJson(const QJsonObject &json)
{
    EphemeralState s;

    // Read our private key. An Obsidian-authored `scroll` (a line number) is
    // left in extraKeys and round-tripped verbatim rather than being
    // misinterpreted as a fraction.
    if (json.contains(QLatin1StringView(kKeyScrollFraction))) {
        s.scroll = static_cast<float>(
            json.value(QLatin1StringView(kKeyScrollFraction)).toDouble(0.0));
    }

    if (json.contains(QLatin1StringView(kKeyCursor))) {
        const auto cursorObj = json.value(QLatin1StringView(kKeyCursor)).toObject();
        s.cursor.line = cursorObj.value(QLatin1StringView(kKeyCursorLine)).toInt(0);
        s.cursor.column = cursorObj.value(QLatin1StringView(kKeyCursorColumn)).toInt(0);
    }

    if (json.contains(QLatin1StringView(kKeyMode))) {
        s.modeRaw = json.value(QLatin1StringView(kKeyMode)).toString(
            QStringLiteral("source"));
    }

    if (json.contains(QLatin1StringView(kKeySource))) {
        const auto v = json.value(QLatin1StringView(kKeySource));
        // Absent or null → default (false). Present non-bool → treat as false.
        s.sourceFlag = v.isBool() ? v.toBool() : false;
    }

    if (json.contains(QLatin1StringView(kKeyFoldedHeadings))) {
        const auto arr = json.value(QLatin1StringView(kKeyFoldedHeadings)).toArray();
        s.foldedHeadings.reserve(arr.size());
        for (const auto &v : arr) {
            if (v.isDouble()) s.foldedHeadings.append(v.toInt());
        }
    }

    // Stash unknown keys.
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        if (!isKnownKey(it.key())) {
            s.extraKeys.insert(it.key(), it.value());
        }
    }

    return s;
}

} // namespace Corbomite
