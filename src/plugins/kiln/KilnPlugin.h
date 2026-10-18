// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

#include <QPointer>

#include <memory>

namespace Corbomite {

class KilnRunner;
class KilnView;

class KilnPlugin : public Plugin
{
    Q_OBJECT
public:
    KilnPlugin(QObject *parent = nullptr, const QVariantList & = {});
    ~KilnPlugin() override;

    QObject *createView(MainWindow *mainWindow) override;

protected:
    void onLoad(PluginContext *ctx) override;

public:
    /// Fired by the host on every change to this plugin's data.json — including the
    /// panel's own saves, which is why the panel compares before it reloads.
    void onExternalSettingsChange() override;

private:
    std::unique_ptr<KilnRunner> m_runner;
    /// The hosted panel, so the commands can drive the same instance the user is
    /// looking at rather than a second copy with its own settings.
    QPointer<KilnView> m_view;
};

} // namespace Corbomite
