// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/NoteMeta.h"
#include "corbomite/storage/IgnoreFilter.h"

#include <QVector>
#include <QString>

namespace Corbomite {

class VaultScanner {
public:
    QVector<NoteMeta> scan(const QString &vaultRoot) const;

    /// Install a user-supplied path filter (from `userIgnoreFilters` in
    /// `app.json`). Matching paths are omitted from scan results.
    void setIgnoreFilter(IgnoreFilter filter) { m_ignore = std::move(filter); }

private:
    bool shouldExcludeDir(const QString &dirName) const;
    bool isNoteFile(const QString &suffix) const;

    IgnoreFilter m_ignore;
};

} // namespace Corbomite
