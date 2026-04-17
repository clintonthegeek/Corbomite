// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class PropertiesPlugin : public Plugin
{
    Q_OBJECT
public:
    PropertiesPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~PropertiesPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;
};

} // namespace Corbomite
