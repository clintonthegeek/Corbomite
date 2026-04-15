// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/VaultProcess.h"

#include "corbomite/storage/DataAdapter.h"

#include <QMutex>
#include <QMutexLocker>

#include <memory>
#include <string>
#include <unordered_map>

namespace Corbomite {

namespace {

// Per-path lock registry. `process()` acquires the lock for its target path
// for the full read-mutate-write cycle so concurrent callers on the same file
// serialise instead of colliding (last-write-wins → lost update).
QMutex &lockForPath(const QString &path)
{
    static QMutex registryMutex;
    static std::unordered_map<std::string, std::unique_ptr<QMutex>> registry;

    QMutexLocker guard(&registryMutex);
    const std::string key = path.toStdString();
    auto it = registry.find(key);
    if (it == registry.end()) {
        it = registry.emplace(key, std::make_unique<QMutex>()).first;
    }
    return *it->second;
}

} // namespace

bool VaultProcess::process(DataAdapter *fs,
                           const QString &absolutePath,
                           const Mutator &mutator)
{
    if (!fs || !mutator || absolutePath.isEmpty()) return false;

    QMutexLocker pathLock(&lockForPath(absolutePath));

    const auto current = fs->read(absolutePath);
    if (!current.has_value()) return false;

    const QString next = mutator(*current);
    return fs->write(absolutePath, next);
}

} // namespace Corbomite
