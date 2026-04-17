// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewPlugin.h"

#include <KPluginFactory>

namespace Corbomite {

GraphViewPlugin::GraphViewPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

GraphViewPlugin::~GraphViewPlugin() = default;

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(GraphViewPluginFactory, "metadata.json",
    registerPlugin<Corbomite::GraphViewPlugin>();)

#include "GraphViewPlugin.moc"
