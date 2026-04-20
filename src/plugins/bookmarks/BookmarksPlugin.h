// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

#include <QPointer>
#include <QTimer>

namespace Corbomite::Bookmarks {

class BookmarksStore;

class BookmarksPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    BookmarksPlugin(QObject *parent, const QVariantList &args);
    ~BookmarksPlugin() override;

    QObject *createView(Corbomite::MainWindow *mainWindow) override;

    QJsonObject saveSessionState(QObject *view) const override;
    void loadSessionState(QObject *view, const QJsonObject &state) override;

protected:
    void onLoad(Corbomite::PluginContext *ctx) override;
    void onUnload() override;

private:
    void registerCommands(Corbomite::PluginContext *ctx);
    void scheduleSave();
    void doSave();

    QPointer<BookmarksStore> m_store;
    QTimer                   m_saveTimer;
};

} // namespace Corbomite::Bookmarks
