// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/NoteMeta.h"
#include <QVector>
#include <QString>

namespace Corbomite {

class VaultScanner {
public:
    QVector<NoteMeta> scan(const QString &vaultRoot) const;

private:
    bool shouldExcludeDir(const QString &dirName) const;
    bool isNoteFile(const QString &suffix) const;
};

} // namespace Corbomite
