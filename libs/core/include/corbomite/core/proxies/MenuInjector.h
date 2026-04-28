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

    /// File-menu handler shape. Mirrors Obsidian's `file-menu` event payload:
    /// `(menu, filePath, source)`. `source` is one of the `FileMenuSource`
    /// constants in `MenuEventEmitter.h` — plugin code uses it to scope a
    /// handler to a specific invocation site (file-explorer right-click vs
    /// tab-header right-click vs link-context, etc.).
    using FileMenuHandler = std::function<void(QMenu *menu,
                                                const QString &filePath,
                                                const QString &source)>;

    explicit MenuInjector(MenuEventEmitter *emitter);
    ~MenuInjector();

    MenuInjector(const MenuInjector &) = delete;
    MenuInjector &operator=(const MenuInjector &) = delete;

    /// Subscribe to MenuEventEmitter::fileMenu. The handler receives the
    /// menu, the file path the menu was opened on, and a source
    /// discriminator (see FileMenuSource constants).
    void onFileMenuBuilt(FileMenuHandler handler);

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
