// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class LocalGraphPlugin : public Plugin
{
    Q_OBJECT
public:
    LocalGraphPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~LocalGraphPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;
};

} // namespace Corbomite
