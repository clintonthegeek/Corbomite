// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/EphemeralState.h"

#include <QJsonArray>
#include <QJsonValue>

namespace Corbomite {

namespace {

// Known top-level keys in the eState object. Anything not in this set is
// copied into `extraKeys` verbatim so round-tripping preserves future
// Obsidian fields / plugin data (Cluster B WorkspaceState idiom).
const char *kKeyScroll         = "scroll";
const char *kKeyCursor         = "cursor";
const char *kKeyCursorLine     = "line";
const char *kKeyCursorColumn   = "column";
const char *kKeyMode           = "mode";
const char *kKeySource         = "source";
const char *kKeyFoldedHeadings = "foldedHeadings";

bool isKnownKey(const QString &key)
{
    return key == QLatin1StringView(kKeyScroll)
        || key == QLatin1StringView(kKeyCursor)
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

    obj.insert(QLatin1StringView(kKeyScroll), static_cast<double>(scroll));

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

    if (json.contains(QLatin1StringView(kKeyScroll))) {
        s.scroll = static_cast<float>(
            json.value(QLatin1StringView(kKeyScroll)).toDouble(0.0));
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
