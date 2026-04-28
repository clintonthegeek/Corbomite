// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "markoff/EmbedRegistry.h"

#include <QString>
#include <QStringList>

namespace Corbomite {

/// Embed-extension registration facade for plugins with the "ui.rendering"
/// permission. Embeds are dispatched by file extension; the registrar
/// tracks every extension it registered and unregisters them all on
/// destruction. Note: extensions are NOT plugin-id-namespaced (they are
/// shared across all plugins by definition — first-registered-wins).
class EmbedRegistrar
{
public:
    explicit EmbedRegistrar(Markoff::EmbedRegistry *registry);
    ~EmbedRegistrar();

    EmbedRegistrar(const EmbedRegistrar &) = delete;
    EmbedRegistrar &operator=(const EmbedRegistrar &) = delete;

    /// Returns false if the extension was already registered by something
    /// else (first-wins); true on success. Caller's factory is held by
    /// the registry until unregister.
    bool registerExtension(const QString &ext, Markoff::EmbedFactory factory);

    void unregisterExtension(const QString &ext);

private:
    Markoff::EmbedRegistry *m_registry;
    QStringList m_registeredExts;
};

} // namespace Corbomite
