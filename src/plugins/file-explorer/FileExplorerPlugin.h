// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class FileExplorerPlugin : public Plugin
{
    Q_OBJECT
public:
    FileExplorerPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~FileExplorerPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;
    QJsonObject saveSessionState(QObject *view) const override;
    void loadSessionState(QObject *view, const QJsonObject &state) override;
};

} // namespace Corbomite
