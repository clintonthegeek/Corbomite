// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_CODEBLOCKPROCESSORREGISTRY_H
#define CORBOMITE_CORE_CODEBLOCKPROCESSORREGISTRY_H

#include <QHash>
#include <QString>

#include <cstdint>
#include <functional>

namespace Corbomite::Core {

class VaultResourceProvider;

/// Context passed to every code-block processor invocation.
struct CodeBlockContext
{
    QString sourcePath;
    VaultResourceProvider *resources = nullptr; ///< not owned
    int depth = 0;
};

// Returns true if handled; false to fall through to default highlighting.
using CodeBlockProcessorFn = std::function<bool(const QString &source,
                                                void *node,
                                                const CodeBlockContext &)>;

// WHY (design, not implementation): per-language dispatch is SYNCHRONOUS.
// Same async-placeholder pattern as PostProcessorRegistry — the processor
// mutates the scenegraph with a placeholder and triggers its own async
// update via signal. Re-evaluate when plugin-authored code-block
// processors surface use cases the placeholder pattern cannot express.
class CodeBlockProcessorRegistry
{
public:
    /// Returned from `registerLanguage`; pass back to `unregister` to
    /// remove the registration. The handle is valid for the lifetime of
    /// this registry.
    struct Handle
    {
        std::uint64_t id = 0;
        QString language; ///< lowercased, matches the registered key
    };

    Handle registerLanguage(const QString &language, CodeBlockProcessorFn fn);
    void unregister(const Handle &h);
    bool dispatch(const QString &language,
                  const QString &source,
                  void *node,
                  const CodeBlockContext &ctx) const;

private:
    struct Entry
    {
        std::uint64_t id = 0;
        CodeBlockProcessorFn fn;
    };
    QHash<QString, Entry> m_byLang; ///< key = lowercased language name
    std::uint64_t m_nextId = 1;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_CODEBLOCKPROCESSORREGISTRY_H
