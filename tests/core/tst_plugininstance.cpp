// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::PluginInstance (Cluster C Phase 3, scaffolded stub).
// Spec: docs/obsidian-audit/domains/plugin.md §3–§4
//
// Preserves Obsidian quirks:
//   - addCommand mutates cmd.id in place to `<pluginId>:<origId>`
//   - A second addCommand call on the same Command object double-
//     namespaces it (`pluginId:pluginId:origId`) — Obsidian bug,
//     mirrored for plugin compat.

#include <QTest>
#include <QObject>

#include "corbomite/core/PluginInstance.h"
#include "corbomite/core/Command.h"

using Corbomite::Command;
using Corbomite::CommandRegistry;
using Corbomite::PluginInstance;

class TestPluginInstance : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testAddCommandPrefixesId()
    {
        CommandRegistry reg;
        PluginInstance plugin(QStringLiteral("myplug"), &reg);

        Command cmd;
        cmd.id = QStringLiteral("do-thing");
        cmd.callback = []() {};
        plugin.addCommand(cmd);

        // Registry has the prefixed id.
        QVERIFY(reg.findCommand(QStringLiteral("myplug:do-thing")));
        // Original id is NOT registered.
        QVERIFY(!reg.findCommand(QStringLiteral("do-thing")));
        // Caller-provided cmd object was mutated (Obsidian in-place quirk).
        QCOMPARE(cmd.id, QStringLiteral("myplug:do-thing"));
    }

    void testAddCommandTwiceDoubleNamespaces()
    {
        // Preserved Obsidian bug: addCommand mutates cmd.id in place, so
        // a second call produces "myplug:myplug:do-thing".
        CommandRegistry reg;
        PluginInstance plugin(QStringLiteral("myplug"), &reg);

        Command cmd;
        cmd.id = QStringLiteral("do-thing");
        cmd.callback = []() {};
        plugin.addCommand(cmd);
        plugin.addCommand(cmd);

        QVERIFY(reg.findCommand(QStringLiteral("myplug:myplug:do-thing")));
    }

    void testRemoveCommandByLocalId()
    {
        CommandRegistry reg;
        PluginInstance plugin(QStringLiteral("myplug"), &reg);

        Command cmd;
        cmd.id = QStringLiteral("go");
        cmd.callback = []() {};
        plugin.addCommand(cmd);
        QVERIFY(plugin.removeCommand(QStringLiteral("go")));
        QVERIFY(!reg.findCommand(QStringLiteral("myplug:go")));
    }

    void testUnloadRemovesAllRegisteredCommands()
    {
        CommandRegistry reg;
        {
            PluginInstance plugin(QStringLiteral("myplug"), &reg);
            plugin.load();

            Command a;
            a.id = QStringLiteral("a");
            a.callback = []() {};
            plugin.addCommand(a);

            Command b;
            b.id = QStringLiteral("b");
            b.callback = []() {};
            plugin.addCommand(b);

            QCOMPARE(reg.listCommands().size(), 2);
            plugin.unload();
            QCOMPARE(reg.listCommands().size(), 0);
        }
    }
};

QTEST_MAIN(TestPluginInstance)
#include "tst_plugininstance.moc"
