// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/Component.h"

#include <QJsonObject>
#include <QObject>

class QWidget;
namespace KTextEditor { class ConfigPage; }

namespace Corbomite {

class PluginContext;
class MainWindow;

/// Abstract base for all Corbomite plugins (built-in and community).
///
/// Multi-inherits QObject (for signals/Q_OBJECT and KPluginFactory's
/// `factory->create<Plugin>(parent)` parent-pointer signature) and
/// Component (for the load/unload + register* lifecycle primitives).
///
/// Subclasses MAY override:
///   - `onLoad(PluginContext*)`  — called once at plugin enable
///   - `onUnload()`              — called once at plugin disable
///   - `createView(MainWindow*)` — per-window UI factory; default returns nullptr
///   - `configPages()` / `configPage(int, QWidget*)` — KConfig pages
///
/// Lifecycle entry-point is `load(PluginContext*)`, NOT `Component::load()`:
/// PluginManager calls `plugin->load(ctx)`, which captures the context and
/// drives Component's lifecycle (firing onLoad → children → cleanups → onUnload).
class Plugin : public QObject, public Component
{
    Q_OBJECT
public:
    explicit Plugin(QObject *parent = nullptr);
    ~Plugin() override;

    /// Capture context, then drive Component::load(). Idempotent.
    void load(PluginContext *ctx);

    /// Hide Component::load() from public callers — they must use load(ctx).
    using Component::unload;
    using Component::isLoaded;

    /// Context handed to this plugin at load. nullptr before load() / after unload().
    PluginContext *context() const { return m_context; }

    /// Per-window view factory. Default returns nullptr (headless plugins).
    virtual QObject *createView(MainWindow *mainWindow);

    /// Hand focus to the plugin's view. Default sets focus on `view` if it's
    /// a QWidget; plugins whose view contains a non-root target (e.g. Search
    /// wants its QLineEdit focused, not the surrounding tree view) override
    /// this to dispatch. Called by the host after `showToolView(...)` when
    /// the user triggers a focus shortcut (e.g. Ctrl+Shift+F for Search).
    virtual void focus(QObject *view);

    /// Serialize per-plugin session state (tree expand state, sidebar scroll,
    /// anything that should survive a vault close/reopen). Host invokes this
    /// on plugin teardown and stores the resulting QJsonObject under
    /// `_corbomite.plugins.<pluginId>` in workspace.json. Default returns an
    /// empty object.
    virtual QJsonObject saveSessionState(QObject *view) const;

    /// Apply previously-serialized session state. Host invokes this once
    /// after `createView(...)` and before the view goes on screen, passing
    /// the object that was most recently saved via `saveSessionState`.
    /// Default is a no-op.
    virtual void loadSessionState(QObject *view, const QJsonObject &state);

    /// Number of KConfig pages this plugin provides. Default 0.
    virtual int configPages() const { return 0; }

    /// KConfig page factory. Default returns nullptr.
    virtual KTextEditor::ConfigPage *configPage(int number, QWidget *parent);

protected:
    /// Override point — called inside load(ctx) before Component::load().
    virtual void onLoad(PluginContext *ctx) { Q_UNUSED(ctx); }

    /// Override point — called from Component::onunload after children/cleanups
    /// have run. Default is no-op.
    virtual void onUnload() {}

    // Bridge Component's lowercase virtuals to Plugin's camelCase override points.
    // Subclasses override `onLoad` / `onUnload`, not these.
    void onload() final;
    void onunload() final;

private:
    // Note: declaring `load(PluginContext*)` hides Component's no-arg load(),
    // so external callers must use the context-aware overload.

    PluginContext *m_context = nullptr;
};

} // namespace Corbomite
