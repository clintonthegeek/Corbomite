// SPDX-License-Identifier: GPL-3.0-or-later
#include "TemplatePlugin.h"

#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KPluginFactory>
#include <QDebug>

TemplatePlugin::TemplatePlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent)
{
}

TemplatePlugin::~TemplatePlugin() = default;

void TemplatePlugin::onLoad(Corbomite::PluginContext *ctx)
{
    if (!ctx) {
        return;
    }
    if (auto *vault = ctx->vault()) {
        qInfo() << "template plugin loaded; vault has"
                << vault->getMarkdownFiles().size() << "markdown files";
    } else {
        qInfo() << "template plugin loaded; vault.read permission not granted "
                   "or no vault open";
    }
}

void TemplatePlugin::onUnload()
{
}

K_PLUGIN_FACTORY_WITH_JSON(TemplatePluginFactory, "metadata.json",
    registerPlugin<TemplatePlugin>();)

#include "TemplatePlugin.moc"
