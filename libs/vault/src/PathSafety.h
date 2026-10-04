// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite::VaultPaths {

/// Obsidian's illegal-path-character set, which is **platform-conditional**
/// (`ND`/`HD`, `app/own/1126-FD.js:52,54`, gated on `Jp = platform === "win"`
/// and `nd = /android/i.test(userAgent)` from `app/own/0519-Qp.js:13,16`):
///
///   - Linux / macOS: `\`, `/`, `:`
///   - Windows:       `*`, `"`, `\`, `/`, `<`, `>`, `:`, `|`, `?`
///   - Android:       `\`, `/`, `:`, `*`, `?`, `<`, `>`, `"` (no `|`)
///
/// The trailing-dot/space and reserved-device rules are Windows-only and
/// live in `FileNameValidator.cpp`; only the character set is here.
///
/// The load-bearing consequence: the POSIX set is `\ / :`, so a name like
/// `Notes?` or `a|b` is a *legal* vault filename that Obsidian-on-Linux
/// accepts, and a cross-tool validator must not reject it. Rejecting the
/// Windows set everywhere is the divergence `checkPath`'s platform gate
/// exists to prevent.
inline const QString &illegalPathChars()
{
    static const QString chars =
#ifdef Q_OS_WIN
        QStringLiteral("\"*/:<>?\\|");
#elif defined(Q_OS_ANDROID)
        QStringLiteral("\"*/:<>?\\");
#else
        QStringLiteral("\\/:");
#endif
    return chars;
}

/// The same set space-separated, for the user-facing error message —
/// Obsidian's `BD = ND.split("").join(" ")` (`app/own/1126-FD.js:53`).
inline QString illegalPathCharsForDisplay()
{
    QStringList parts;
    for (const QChar ch : illegalPathChars()) parts.append(QString(ch));
    return parts.join(QLatin1Char(' '));
}

/// True if `name` contains a character illegal in a path on this platform.
/// Obsidian applies this test per `/`-delimited segment in `checkPath`
/// (`app/own/1128-jD.js:36-37`) — which makes the `/` in the set dead once
/// `normalizePath` has run — and against the whole string in
/// `validateConfigDir` (`app/own/1129-UD.js:105-108`). Callers that pass an
/// already-normalised name see the POSIX/Windows sets above with the `/`
/// harmlessly present.
inline bool hasIllegalPathChar(const QString &name)
{
    for (const QChar ch : name) {
        if (illegalPathChars().contains(ch)) return true;
    }
    return false;
}

} // namespace Corbomite::VaultPaths