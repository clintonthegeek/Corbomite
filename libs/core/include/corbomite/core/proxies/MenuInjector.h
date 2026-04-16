// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <functional>

class QMenu;

namespace Corbomite {

class MenuEventEmitter;

/// Menu-injection facade for plugins with the "ui.menus" permission.
/// Subscribes to MenuEventEmitter signals; auto-disconnects on plugin unload.
/// Stub — wire-up lands in Cluster Q Task 9.
class MenuInjector
{
public:
    explicit MenuInjector(MenuEventEmitter *emitter) : m_emitter(emitter) {}

    using Handler = std::function<void(QMenu *, const QString &)>;
    void onFileMenuBuilt(Handler handler);
    void onEditorMenuBuilt(Handler handler);
    void onTabMenuBuilt(Handler handler);

private:
    MenuEventEmitter *m_emitter;
};

} // namespace Corbomite
