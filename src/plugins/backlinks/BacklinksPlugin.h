// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class BacklinksPlugin : public Plugin
{
    Q_OBJECT
public:
    BacklinksPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~BacklinksPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;
};

} // namespace Corbomite
