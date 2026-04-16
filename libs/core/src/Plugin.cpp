// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Plugin.h"

namespace Corbomite {

Plugin::Plugin(QObject *parent) : QObject(parent), Component() {}
Plugin::~Plugin() = default;

void Plugin::load(PluginContext *ctx)
{
    if (isLoaded()) return;
    m_context = ctx;
    Component::load(); // fires onload() → onLoad(m_context), then children
}

void Plugin::onload()
{
    onLoad(m_context);
}

void Plugin::onunload()
{
    onUnload();
    m_context = nullptr;
}

QObject *Plugin::createView(MainWindow *)
{
    return nullptr;
}

KTextEditor::ConfigPage *Plugin::configPage(int, QWidget *)
{
    return nullptr;
}

} // namespace Corbomite
