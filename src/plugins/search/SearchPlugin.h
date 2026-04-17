// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

namespace Corbomite {

class SearchPlugin : public Plugin
{
    Q_OBJECT
public:
    SearchPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~SearchPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;
};

} // namespace Corbomite
