// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteStatsPlugin.h"
#include "NoteStatsView.h"

#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KPluginFactory>
#include <QDebug>

namespace NoteStats {

NoteStatsPlugin::NoteStatsPlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent) {}

NoteStatsPlugin::~NoteStatsPlugin() = default;

QObject *NoteStatsPlugin::createView(Corbomite::MainWindow *mw)
{
    auto *ctx = context();
    if (!ctx) return nullptr;
    auto *vault = ctx->vault();
    auto *search = ctx->search();
    auto *metadata = ctx->metadataCache();
    if (!vault || !search || !metadata) {
        qWarning() << "note-stats: missing permissions; skipping view";
        return nullptr;
    }
    return new NoteStatsView(vault, search, metadata,
                             reinterpret_cast<QWidget *>(mw));
}

} // namespace NoteStats

K_PLUGIN_FACTORY_WITH_JSON(NoteStatsPluginFactory, "metadata.json",
    registerPlugin<NoteStats::NoteStatsPlugin>();)

#include "NoteStatsPlugin.moc"
