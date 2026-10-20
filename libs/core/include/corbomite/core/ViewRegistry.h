// libs/core/include/corbomite/core/ViewRegistry.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <functional>

namespace Corbomite {

class NoteDocument;
class View;
class ViewActions;
class WorkspaceLeaf;

class ViewRegistry : public QObject
{
    Q_OBJECT

public:
    using ViewFactory = std::function<View *(WorkspaceLeaf *)>;
    using FileResolver = std::function<NoteDocument *(const QString &relativePath)>;

    explicit ViewRegistry(QObject *parent = nullptr);

    void setFileResolver(FileResolver resolver);
    NoteDocument *resolveFile(const QString &relativePath) const;

    void registerView(const QString &type, ViewFactory factory);
    void unregisterView(const QString &type);

    void registerExtensions(const QStringList &exts, const QString &type);
    void unregisterExtensions(const QStringList &exts);

    void registerViewWithExtensions(const QStringList &exts, const QString &type,
                                    ViewFactory factory);

    // ---- ViewActions providers ---------------------------------------
    //
    // Graph's view type is plugin-registered, so its provider has to travel
    // the same path as the view itself; this is what lets a plugin ship a
    // ViewActions provider with no new plugin-API surface.
    //
    // LIFETIME IS THE WHOLE STORY. A provider built inside a plugin .so has
    // a vtable in that .so, so it MUST be unregistered before the library
    // unloads — an installed provider whose .so is gone is a guaranteed
    // crash on the next vtable call, and KXMLGUIFactory would still be
    // holding the client. Therefore:
    //   * the registry does NOT take ownership (it cannot outlive the .so);
    //   * it stores a QPointer so a provider destroyed without a matching
    //     unregister cannot leave a dangling key;
    //   * the plugin is expected to call ViewRegistrar::unregisterViewActions
    //     from its onUnload(), which is the one hook that provably runs
    //     before the library goes away. The registrar destructor is the
    //     belt-and-braces fallback, not the primary path.
    void registerViewActions(const QString &type, ViewActions *provider);
    void unregisterViewActions(const QString &type);
    ViewActions *viewActionsForType(const QString &type) const;
    /// View types with a live provider. Lets a late observer (the window's
    /// ActionContextController) adopt everything registered before it was
    /// connected, instead of only seeing registrations that happen later.
    QStringList registeredViewActionTypes() const;

    ViewFactory getViewCreatorByType(const QString &type) const;
    QString getTypeByExtension(const QString &ext) const;
    bool isExtensionRegistered(const QString &ext) const;

Q_SIGNALS:
    void viewRegistered(const QString &type);
    void viewUnregistered(const QString &type);
    void extensionsUpdated();

    /// `provider` is QPointer<> on the wire deliberately: the sender side
    /// may outlive the object if a plugin is torn down abruptly, and a
    /// controller that trusted a stale pointer would crash.
    void viewActionsRegistered(const QString &type, QPointer<ViewActions> provider);
    void viewActionsUnregistered(const QString &type);

private:
    QHash<QString, ViewFactory> m_viewByType;
    QHash<QString, QPointer<ViewActions>> m_viewActionsByType;
    QHash<QString, QString> m_typeByExtension;
    FileResolver m_fileResolver;
};

} // namespace Corbomite
