// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaObject>
#include <QString>
#include <QVector>

#include <functional>

class QMenu;

namespace Corbomite {

class MenuEventEmitter;

/// Menu-injection facade for plugins with the "ui.menus" permission.
///
/// Each on*MenuBuilt() call subscribes to the matching MenuEventEmitter
/// signal; the proxy stores the QMetaObject::Connection and disconnects
/// on destruction. Permission gating happens upstream in
/// PluginContext::menus().
class MenuInjector
{
public:
    using Handler = std::function<void(QMenu *menu, const QString &context)>;

    explicit MenuInjector(MenuEventEmitter *emitter);
    ~MenuInjector();

    MenuInjector(const MenuInjector &) = delete;
    MenuInjector &operator=(const MenuInjector &) = delete;

    /// Subscribe to MenuEventEmitter::fileMenu. The handler's `context`
    /// argument receives the file path the menu was opened on.
    void onFileMenuBuilt(Handler handler);

    /// Subscribe to MenuEventEmitter::editorMenu. The handler's `context`
    /// argument is the address of the QObject editor as a hex string
    /// (plugins receive opaque ids; raw QObject pointers are not exposed
    /// across the plugin surface).
    void onEditorMenuBuilt(Handler handler);

    /// Subscribe to MenuEventEmitter::leafMenu (Obsidian's "tab menu").
    /// The handler's `context` argument is the leaf id as a hex string.
    void onTabMenuBuilt(Handler handler);

private:
    MenuEventEmitter *m_emitter;
    QVector<QMetaObject::Connection> m_connections;
};

} // namespace Corbomite
