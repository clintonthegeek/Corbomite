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
    auto *search = ctx->search();
    auto *metadata = ctx->metadataCache();
    if (!search || !metadata) {
        qWarning() << "SearchPlugin: metadata.read missing; view skipped";
        return nullptr;
    }
    return new SearchView(search, metadata, ctx->workspace(),
                            reinterpret_cast<QWidget *>(mainWindow));
}

void SearchPlugin::focus(QObject *view)
{
    if (auto *sv = qobject_cast<SearchView *>(view)) {
        sv->focusSearchInput();
        return;
    }
    Plugin::focus(view);
}

} // namespace Corbomite

K_PLUGIN_FACTORY_WITH_JSON(SearchPluginFactory, "metadata.json",
    registerPlugin<Corbomite::SearchPlugin>();)

#include "SearchPlugin.moc"
