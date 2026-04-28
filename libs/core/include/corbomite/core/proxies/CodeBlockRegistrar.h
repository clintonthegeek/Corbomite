// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "markoff/CodeBlockProcessorRegistry.h"

#include <QString>
#include <QStringList>

namespace Corbomite {

/// Code-block-language registration facade for plugins with the
/// "ui.rendering" permission. Code-block processors are dispatched by
/// language tag (e.g. "mermaid", "math"); the registrar tracks every
/// language it registered and unregisters them all on destruction.
/// Note: language tags are NOT plugin-id-namespaced (they are shared
/// across all plugins — first-registered-wins).
class CodeBlockRegistrar
{
public:
    explicit CodeBlockRegistrar(Markoff::CodeBlockProcessorRegistry *registry);
    ~CodeBlockRegistrar();

    CodeBlockRegistrar(const CodeBlockRegistrar &) = delete;
    CodeBlockRegistrar &operator=(const CodeBlockRegistrar &) = delete;

    /// Returns false if the language was already registered (first-wins),
    /// true on success.
    bool registerLanguage(const QString &lang, Markoff::CodeBlockProcessor proc);

    void unregisterLanguage(const QString &lang);

private:
    Markoff::CodeBlockProcessorRegistry *m_registry;
    QStringList m_registeredLangs;
};

} // namespace Corbomite
