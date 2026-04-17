// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

#include <QPointer>

namespace Corbomite {

class GraphControlsPanel;
class GraphView;
class MetadataCache;
class SQLiteIndex;
class Vault;

/// Cluster Q follow-up — full GraphView plugin.
///
/// onLoad captures core services (Vault, SQLiteIndex, MetadataCache) from
/// the PluginContext and registers the "graph" view type with
/// ViewRegistrar. The registered factory spawns a GraphView bound to
/// those services and wires its GraphViewTab to this plugin's controls
/// panel. createView returns the shared GraphControlsPanel, hosted by
/// MainWindow as a Right-side tool view (via the standard plugin
/// hosting path — see metadata.json's X-Corbomite-Dock*).
///
/// Unload unregisters the view type (destroys the controls panel via
/// its MainWindow tool-view parent on plugin teardown).
class GraphViewPlugin : public Plugin
{
    Q_OBJECT
public:
    GraphViewPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~GraphViewPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;

protected:
    void onLoad(PluginContext *ctx) override;
    void onUnload() override;

private:
    // Non-owning — core services captured at load, used by the factory
    // closure registered with ViewRegistrar.
    Vault         *m_vault = nullptr;
    SQLiteIndex   *m_index = nullptr;
    MetadataCache *m_metadata = nullptr;

    // Shared controls panel. Host-owned (re-parented into the tool view
    // when createView hands it out). QPointer so teardown tolerates
    // reparent-delete races.
    QPointer<GraphControlsPanel> m_controlsPanel;
};

} // namespace Corbomite
