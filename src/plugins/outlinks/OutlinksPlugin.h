// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class OutlinksPlugin : public Plugin
{
    Q_OBJECT
public:
    OutlinksPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~OutlinksPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;

protected:
    void onLoad(PluginContext *ctx) override;
};

} // namespace Corbomite
