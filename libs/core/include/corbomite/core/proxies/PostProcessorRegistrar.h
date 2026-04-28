// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PostProcessorRegistry.h"

#include <QVector>

namespace Corbomite {

/// Markdown-post-processor registration facade for plugins with the
/// "ui.rendering" permission. PostProcessorRegistry hands out opaque
/// Handles; the registrar tracks them and unregisters all on destruction.
class PostProcessorRegistrar
{
public:
    explicit PostProcessorRegistrar(Corbomite::Core::PostProcessorRegistry *registry);
    ~PostProcessorRegistrar();

    PostProcessorRegistrar(const PostProcessorRegistrar &) = delete;
    PostProcessorRegistrar &operator=(const PostProcessorRegistrar &) = delete;

    Corbomite::Core::PostProcessorRegistry::Handle
    registerProcessor(int priority, Corbomite::Core::PostProcessorFn fn);

    void unregister(Corbomite::Core::PostProcessorRegistry::Handle handle);

private:
    Corbomite::Core::PostProcessorRegistry *m_registry;
    QVector<Corbomite::Core::PostProcessorRegistry::Handle> m_handles;
};

} // namespace Corbomite
