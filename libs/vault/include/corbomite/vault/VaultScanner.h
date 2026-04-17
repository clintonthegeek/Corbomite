// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/NoteMeta.h"
#include "corbomite/storage/IgnoreFilter.h"

#include <QVector>
#include <QString>

// Moved from libs/storage/ into libs/vault/ during Cluster Q.0 Phase 2 Task
// 2.1; header path is now corbomite/vault/VaultScanner.h. Namespace remains
// Corbomite:: — no source changes at callers other than the #include path.

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
