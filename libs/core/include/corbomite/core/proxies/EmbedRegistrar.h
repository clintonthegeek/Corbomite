// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// TODO(port-foundation-exploration): Markoff::EmbedRegistry / Markoff::EmbedFactory
// retired with the old leaves. Registrar class stubbed to forward-decl
// placeholders so the header compiles; consumers that actually register embeds
// will need their own stubs until E3 lands the new EmbedRegistry abstract.
// #include "markoff/EmbedRegistry.h"

#include <QString>
#include <QStringList>
#include <functional>

namespace Markoff {
class EmbedRegistry;  // stub forward decl — type undefined post-port
using EmbedFactory = std::function<void()>;  // stub — real type unknown post-port
} // namespace Markoff

namespace Corbomite {

class EmbedRegistrar
{
public:
    explicit EmbedRegistrar(Markoff::EmbedRegistry *registry);
    ~EmbedRegistrar();

    EmbedRegistrar(const EmbedRegistrar &) = delete;
    EmbedRegistrar &operator=(const EmbedRegistrar &) = delete;

    bool registerExtension(const QString &ext, Markoff::EmbedFactory factory);
    void unregisterExtension(const QString &ext);

private:
    Markoff::EmbedRegistry *m_registry;
    QStringList m_registeredExts;
};

} // namespace Corbomite
