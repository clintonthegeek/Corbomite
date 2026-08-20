// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/CaseSensitivityProbe.h"

#include "corbomite/core/DataAdapter.h"

#include <QUuid>

namespace Corbomite {

bool CaseSensitivityProbe::isCaseSensitive(DataAdapter *fs, const QString &probeDir)
{
    if (!fs || probeDir.isEmpty()) return true;

    // Unique per-invocation name to avoid stomping a concurrent probe.
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString upperName = QStringLiteral(".case-probe-") + token.toUpper();
    const QString lowerName = QStringLiteral(".case-probe-") + token.toLower();

    const QString upperPath = probeDir + QLatin1Char('/') + upperName;
    const QString lowerPath = probeDir + QLatin1Char('/') + lowerName;

    if (!fs->write(upperPath, QStringLiteral("probe"))) {
        return true; // conservative default
    }

    const bool seenAsLower = fs->exists(lowerPath);
    fs->remove(upperPath);
    // Case-insensitive FS: the lowercase path resolved to the same file → visible.
    // Case-sensitive FS: the lowercase path is a distinct (non-existent) file.
    return !seenAsLower;
}

} // namespace Corbomite
