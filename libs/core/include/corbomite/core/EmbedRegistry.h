// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_EMBEDREGISTRY_H
#define CORBOMITE_CORE_EMBEDREGISTRY_H

#include <QHash>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

namespace Corbomite::Core {

class MarkdownRenderChild;
class VaultResourceProvider;

/// Input to an EmbedFactory. Renderers assemble this when they encounter
/// `![[Target#sub]]` (or equivalent) in the parsed document.
struct EmbedRequest
{
    QString targetPath;                          ///< e.g. "Note.md", "image.png"
    QString subpath;                             ///< "#heading" / "#^blockid" / empty
    VaultResourceProvider *resources = nullptr;  ///< not owned
    int depth = 0;                               ///< current embed depth
};

using EmbedFactory =
    std::function<std::unique_ptr<MarkdownRenderChild>(const EmbedRequest &)>;

/// Extension-to-factory dispatch for `![[file.ext]]` embeds.
///
/// Extension keys are case-insensitive (stored lowercased; dispatch
/// lowercases the queried filename suffix). Built-in registrations land
/// in Phase 5; plugin-level registrations will later route through the
/// Cluster N stable ABI.
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
    std::unique_ptr<MarkdownRenderChild> dispatch(const EmbedRequest &req) const;

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
