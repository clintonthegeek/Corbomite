// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_EMBEDREGISTRY_H
#define CORBOMITE_CORE_EMBEDREGISTRY_H

#include "corbomite/core/MarkdownRenderChild.h"

#include <markoff/EmbedRegistry.h>

#include <QHash>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

namespace Corbomite::Core {

/// Phase C1: `EmbedRequest` and `EmbedFactory` are type aliases for the
/// Markoff DI-seam types. `Corbomite::Core::EmbedRequest` and
/// `Markoff::EmbedRequest` are the same type — brace-init and factory
/// lambdas remain source-compatible with their pre-C1 forms because the
/// struct shape (targetPath / subpath / resources / depth) matches
/// exactly, and `Corbomite::Core::VaultResourceProvider *` implicitly
/// up-casts to `Markoff::ResourceProvider *` via the inheritance
/// added in `VaultResourceProvider.h`.
using EmbedRequest = Markoff::EmbedRequest;
using EmbedFactory = Markoff::EmbedFactory;

/// Extension-to-factory dispatch for `![[file.ext]]` embeds.
///
/// Corbomite's registry keeps a Handle-based API (used by
/// `tst_embedregistry` + plugin follow-ups) on top of the Markoff
/// interface. Adapters in `corbomite/markoff_adapters/` wrap this
/// registry for consumption by `Markoff::Reading::ReadingView`'s
/// setter-injected seam.
class EmbedRegistry
{
public:
    /// Returned from `registerExtension`; pass back to `unregister` to
    /// remove the registration. The handle is valid for the lifetime of
    /// this registry.
    struct Handle
    {
        std::uint64_t id = 0;
        QString extension; ///< lowercased, matches the registered key
    };

    Handle registerExtension(const QString &extension, EmbedFactory fn);
    void unregister(const Handle &h);
    std::unique_ptr<Markoff::MarkdownRenderChild>
    dispatch(const EmbedRequest &req) const;

private:
    struct Entry
    {
        std::uint64_t id = 0;
        EmbedFactory fn;
    };
    QHash<QString, Entry> m_byExt; ///< key = lowercased extension
    std::uint64_t m_nextId = 1;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_EMBEDREGISTRY_H
