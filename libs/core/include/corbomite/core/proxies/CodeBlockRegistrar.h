// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "markoff/core/CodeBlockProcessorRegistry.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace Corbomite {

/// Code-block-language registration facade for plugins with the
/// "ui.rendering" permission. Code-block processors are dispatched by
/// language tag (e.g. "mermaid", "math"); the registrar tracks every
/// language it registered and unregisters them all on destruction.
/// Note: language tags are NOT plugin-id-namespaced (they are shared
/// across all plugins — first-registered-wins).
///
/// Migrated 2026-05-20 from the old by-value CodeBlockProcessor API to
/// the new shared_ptr-based abstract; old proc.language() identifies the
/// language tag, so the lang parameter is kept for backwards-compatible
/// caller-side semantics and verified against proc->language().
class CodeBlockRegistrar
{
public:
    explicit CodeBlockRegistrar(Markoff::CodeBlockProcessorRegistry *registry);
    ~CodeBlockRegistrar();

    CodeBlockRegistrar(const CodeBlockRegistrar &) = delete;
    CodeBlockRegistrar &operator=(const CodeBlockRegistrar &) = delete;

    /// Returns false if the language was already registered (first-wins),
    /// true on success.
    bool registerLanguage(const QString &lang,
                          std::shared_ptr<Markoff::CodeBlockProcessor> proc);

    void unregisterLanguage(const QString &lang);

private:
    Markoff::CodeBlockProcessorRegistry *m_registry;
    QStringList m_registeredLangs;
};

} // namespace Corbomite
