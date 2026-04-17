// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchPlugin.h"

#include "SearchView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/PluginContext.h"

#include <KPluginFactory>
#include <QDebug>

namespace Corbomite {

SearchPlugin::SearchPlugin(QObject *parent, const QVariantList &)
    : Plugin(parent) {}

SearchPlugin::~SearchPlugin() = default;

QObject *SearchPlugin::createView(MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *index = ctx->searchIndex();
    auto *metadata = ctx->metadataCache();
    if (!index || !metadata) {
        qWarning() << "SearchPlugin: metadata.read missing; view skipped";
        return nullptr;
    }
    return new SearchView(index, metadata, ctx->workspace(),
                            reinterpret_cast<QWidget *>(mainWindow));
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(SearchPluginFactory, "metadata.json",
    registerPlugin<Corbomite::SearchPlugin>();)

#include "SearchPlugin.moc"
