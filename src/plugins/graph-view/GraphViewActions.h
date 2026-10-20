// src/plugins/graph-view/GraphViewActions.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ViewActions.h"

#include <QMetaObject>

namespace Corbomite {

class GraphView;
class View;

/// The graph's `ViewActions` provider.
///
/// Lives INSIDE the plugin, unlike every other provider (markdown, canvas and
/// bases are all host-side, registered from MainWindow). That is the whole
/// point: it proves a plugin can ship a ViewActions provider for its own
/// view type without any new plugin-API surface — the provider travels
/// the existing "ui.views" ViewRegistrar that already carried the view type.
///
/// LIFETIME. Everything in this class is compiled into the plugin .so, so the
/// provider must be handed back to `ViewRegistrar` before the library
/// unloads, or KXMLGUIFactory would keep a client whose vtable is gone.
/// `GraphViewPlugin::onUnload()` does exactly that; the registrar destructor
/// is only the fallback. See ViewRegistry::registerViewActions.
class GraphViewActions : public ViewActions
{
    Q_OBJECT
public:
    /// `commandDispatch` routes the two verbs that already exist as
    /// CommandRegistry commands (`copy-screenshot`, and the local-graph
    /// command issued by the host) rather than duplicating their bodies —
    /// delegate, don't reimplement.
    using CommandDispatch = std::function<void(const QString &commandId)>;

    GraphViewActions(CommandDispatch commandDispatch, QObject *parent = nullptr);

    QString viewType() const override;
    void bind(View *view) override;
    void unbind() override;
    void refresh() override;
    QList<QAction *> toolBarActions() const override;

private:
    void setupActions();
    GraphView *graph() const { return m_boundView; }

    CommandDispatch m_commandDispatch;
    GraphView *m_boundView = nullptr;
    QMetaObject::Connection m_contextConnection;
};

} // namespace Corbomite