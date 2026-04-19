// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class OutlinePlugin : public Plugin
{
    Q_OBJECT
public:
    OutlinePlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~OutlinePlugin() override;

    QObject *createView(MainWindow *mainWindow) override;

protected:
    void onLoad(PluginContext *ctx) override;
};

} // namespace Corbomite
