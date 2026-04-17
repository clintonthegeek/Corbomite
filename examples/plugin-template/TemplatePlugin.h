// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

/// Minimum Corbomite plugin. A real implementation would subclass Plugin
/// and override onLoad / onUnload / createView.
class TemplatePlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    TemplatePlugin(QObject *parent = nullptr, const QVariantList &args = {});
    ~TemplatePlugin() override;

protected:
    void onLoad(Corbomite::PluginContext *ctx) override;
    void onUnload() override;
};
