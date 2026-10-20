// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

#include <QPointer>
#include <QVector>

namespace Corbomite {

class CommandRegistrar;
class GraphControlsPanel;
class GraphView;
class GraphViewActions;
class MetadataCacheReader;
class SearchProxy;
class ViewRegistrar;
class VaultProxy;

/// Full GraphView plugin.
///
/// onLoad captures permission-gated proxies (VaultProxy, SearchProxy,
/// MetadataCacheReader) from the PluginContext and registers the
/// "graph" view type with ViewRegistrar. The registered factory spawns
/// a GraphView bound to those proxies and wires its GraphViewTab to
/// this plugin's controls panel (RETIRED — createView returns
/// nullptr and each GraphViewTab owns its own). Kept as a comment trail:
/// GraphControlsPanel, hosted by MainWindow as a Right-side tool view
/// (via the standard plugin hosting path — see metadata.json's
/// X-Corbomite-Dock*).
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
    // Non-owning — proxies captured at load, used by the factory closure
    // registered with ViewRegistrar. Proxy lifetimes belong to
    // PluginContext.
    VaultProxy          *m_vault = nullptr;
    SearchProxy         *m_search = nullptr;
    MetadataCacheReader *m_metadata = nullptr;

    // Shared controls panel. Host-owned (re-parented into the tool view
    // when createView hands it out). QPointer so teardown tolerates

    // Every GraphView instance appends itself here
    // during its onOpen. The `copy-screenshot` command grabs the most
    // recently active (last in the list) view's GraphViewTab pixmap. Uses
    // QPointer so teardowns tolerate view-deletion races.
    QVector<QPointer<GraphView>> m_views;

    // The provider and the registrar it travelled through.
    // Both held as QPointer/weak-ish so an abnormal teardown cannot
    // dereference a registrar the host already destroyed; onUnload() clears
    // them explicitly in the safe order regardless.
    QPointer<GraphViewActions> m_graphActions;
    ViewRegistrar *m_viewsProxy = nullptr;
    CommandRegistrar *m_commandsProxy = nullptr;
};

} // namespace Corbomite
