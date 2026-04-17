// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include <KPluginMetaData>

#include "../TemplatePlugin.h"

#include "corbomite/core/PluginMetaData.h"
#include "corbomite/vault/PluginContext.h"

using namespace Corbomite;

class TestTemplatePlugin : public QObject
{
    Q_OBJECT
private slots:
    void loadsAndUnloadsCleanly();
};

static PluginMetaData makeMeta()
{
    return PluginMetaData(KPluginMetaData{});
}

void TestTemplatePlugin::loadsAndUnloadsCleanly()
{
    PluginContext ctx(makeMeta(), {QStringLiteral("vault.read")});
    // setCoreServices takes 9 args post-Task 2.6:
    //   vault, fileManager, metadata, searchIndex, workspace,
    //   commands, views, menus, network.
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr, nullptr);

    TemplatePlugin plugin;
    plugin.load(&ctx);
    // Without a vault installed, VaultProxy is null; plugin handles this.
    plugin.unload();
    QVERIFY(true);
}

QTEST_MAIN(TestTemplatePlugin)
#include "tst_template_plugin.moc"
