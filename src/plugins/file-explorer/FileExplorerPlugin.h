// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class FileExplorerView;

class FileExplorerPlugin : public Plugin
{
    Q_OBJECT
public:
    FileExplorerPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~FileExplorerPlugin() override;

    void onLoad(PluginContext *ctx) override;

    QObject *createView(MainWindow *mainWindow) override;
    QJsonObject saveSessionState(QObject *view) const override;
    void loadSessionState(QObject *view, const QJsonObject &state) override;

private:
    // Tracks the most recently created view so the reveal-file command
    // callback has a target. Cleared on view destruction.
    FileExplorerView *m_view = nullptr;
};

} // namespace Corbomite
