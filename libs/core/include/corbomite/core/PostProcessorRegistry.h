// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_POSTPROCESSORREGISTRY_H
#define CORBOMITE_CORE_POSTPROCESSORREGISTRY_H

#include <QString>

#include <cstdint>
#include <functional>
#include <vector>

namespace Corbomite::Core {

class VaultResourceProvider;

/// Context passed to every post-processor invocation.
struct PostProcessorContext
{
    QString sourcePath;
    VaultResourceProvider *resources = nullptr; ///< not owned
    int depth = 0;
};

// Scenegraph node type is renderer-agnostic at the interface level —
// ReadingView passes ReadingSection*, Markoff passes its own node type.
// The interface uses void* and each renderer casts to its expected type
// at its own registration sites (internal API; type safety at wrap layer).
using PostProcessorFn =
    std::function<void(void *node, const PostProcessorContext &)>;

// WHY (design, not implementation): post-processors are SYNCHRONOUS by
// contract. Async work (e.g. Mermaid diagram rendering) is modelled via
// "mutate scenegraph with placeholder, kick off async job, update node
// through the scenegraph's own signal/slot plumbing". The pipeline does
// NOT await futures before declaring a section rendered. This matches
// ReadingView's existing async patterns for math/mermaid/images.
// Revisit after real use surfaces cases the placeholder pattern cannot
// express cleanly.
class PostProcessorRegistry
{
public:
    /// Returned from `registerProcessor`; pass back to `unregister` to
    /// remove the registration. The handle is valid for the lifetime of
    /// this registry.
    struct Handle
    {
        std::uint64_t id = 0;
    };

    Handle registerProcessor(int priority, PostProcessorFn fn);
    void unregister(Handle h);
    void run(void *node, const PostProcessorContext &ctx) const;

private:
    struct Entry
    {
        std::uint64_t id = 0;
        int priority = 0;
        std::uint64_t seq = 0;
        PostProcessorFn fn;
    };
    mutable std::vector<Entry> m_entries;
    mutable bool m_dirty = false;
    std::uint64_t m_nextId = 1;
    std::uint64_t m_nextSeq = 0;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_POSTPROCESSORREGISTRY_H
