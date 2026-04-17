// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

/// Cluster Q Task 20 — shell plugin for the global Graph view.
///
/// This is a *shell* InternalPlugin: it ships as an installed `.so` and
/// loads at vault open like the other Cluster Q plugins, but unlike
/// the sidebar plugins it has no `createView()` widget. The actual
/// GraphView main-area tab still lives in CorbomiteApp; full main-area
/// view-type registration via `ViewRegistrar::registerView("graph", …)`
/// is a follow-up — moving GraphView + GraphViewTab + GraphControlsPanel
/// into the plugin .so requires extracting them from CorbomiteApp's
/// graph/ source set, which is bigger than the time budget of this
/// closeout commit allowed.
class GraphViewPlugin : public Plugin
{
    Q_OBJECT
public:
    GraphViewPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~GraphViewPlugin() override;
};

} // namespace Corbomite
