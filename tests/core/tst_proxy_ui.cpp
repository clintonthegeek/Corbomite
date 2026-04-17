// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QMenu>

#include "corbomite/core/Command.h"
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/MenuInjector.h"
#include "corbomite/core/proxies/ViewRegistrar.h"

using namespace Corbomite;

namespace {

class StubView : public View
{
    Q_OBJECT
public:
    explicit StubView(WorkspaceLeaf *leaf) : View(leaf, nullptr) {}
    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
    QJsonObject getState() const override { return {}; }
    void setState(const QJsonObject &) override {}
};

} // namespace

class TestProxyUi : public QObject
{
    Q_OBJECT

private slots:
    // CommandRegistrar
    void commandAddNamespacesId();
    void commandRemoveByLocalId();
    void commandsCleanedUpOnDestroy();
    void commandsHandleNullRegistry();

    // ViewRegistrar
    void viewRegistrationsForwarded();
    void viewExtensionsForwarded();
    void viewRegistrationsCleanedOnDestroy();
    void viewExtensionsCleanedOnDestroy();
    void viewHandlesNullRegistry();

    // MenuInjector
    void fileMenuHandlerInvokedOnEmit();
    void editorMenuHandlerInvokedOnEmit();
    void tabMenuHandlerInvokedOnEmit();
    void menuHandlersDisconnectedOnDestroy();
    void menuHandlesNullEmitter();
};

// ---------- CommandRegistrar ---------------------------------------------

void TestProxyUi::commandAddNamespacesId()
{
    CommandRegistry registry;
    CommandRegistrar reg(&registry, QStringLiteral("my-plugin"));

    Command cmd;
    cmd.id = QStringLiteral("hello");
    cmd.name = QStringLiteral("Hello");
    cmd.callback = [] {};
    reg.addCommand(cmd);

    QCOMPARE(cmd.id, QStringLiteral("my-plugin:hello"));
    QVERIFY(registry.findCommand(QStringLiteral("my-plugin:hello")) != nullptr);
}

void TestProxyUi::commandRemoveByLocalId()
{
    CommandRegistry registry;
    CommandRegistrar reg(&registry, QStringLiteral("my-plugin"));

    Command cmd;
    cmd.id = QStringLiteral("foo");
    cmd.callback = [] {};
    reg.addCommand(cmd);

    QVERIFY(reg.removeCommand(QStringLiteral("foo")));
    QCOMPARE(registry.findCommand(QStringLiteral("my-plugin:foo")), nullptr);
    QVERIFY(!reg.removeCommand(QStringLiteral("foo"))); // already gone
}

void TestProxyUi::commandsCleanedUpOnDestroy()
{
    CommandRegistry registry;
    {
        CommandRegistrar reg(&registry, QStringLiteral("my-plugin"));
        Command a, b;
        a.id = QStringLiteral("a");
        b.id = QStringLiteral("b");
        a.callback = b.callback = [] {};
        reg.addCommand(a);
        reg.addCommand(b);
        QVERIFY(registry.findCommand(QStringLiteral("my-plugin:a")) != nullptr);
        QVERIFY(registry.findCommand(QStringLiteral("my-plugin:b")) != nullptr);
    }
    QCOMPARE(registry.findCommand(QStringLiteral("my-plugin:a")), nullptr);
    QCOMPARE(registry.findCommand(QStringLiteral("my-plugin:b")), nullptr);
}

void TestProxyUi::commandsHandleNullRegistry()
{
    CommandRegistrar reg(nullptr, QStringLiteral("p"));
    Command cmd;
    cmd.id = QStringLiteral("x");
    reg.addCommand(cmd); // must not crash; id stays unmodified
    QCOMPARE(cmd.id, QStringLiteral("x"));
    QVERIFY(!reg.removeCommand(QStringLiteral("x")));
}

// ---------- ViewRegistrar ------------------------------------------------

void TestProxyUi::viewRegistrationsForwarded()
{
    ViewRegistry registry;
    ViewRegistrar reg(&registry);

    reg.registerView(QStringLiteral("stub"),
                     [](WorkspaceLeaf *leaf) -> View * { return new StubView(leaf); });
    QVERIFY(registry.getViewCreatorByType(QStringLiteral("stub")) != nullptr);
}

void TestProxyUi::viewExtensionsForwarded()
{
    ViewRegistry registry;
    ViewRegistrar reg(&registry);

    reg.registerExtensions({QStringLiteral("foo"), QStringLiteral("bar")},
                           QStringLiteral("stub"));
    QCOMPARE(registry.getTypeByExtension(QStringLiteral("foo")),
             QStringLiteral("stub"));
    QCOMPARE(registry.getTypeByExtension(QStringLiteral("bar")),
             QStringLiteral("stub"));
}

void TestProxyUi::viewRegistrationsCleanedOnDestroy()
{
    ViewRegistry registry;
    {
        ViewRegistrar reg(&registry);
        reg.registerView(QStringLiteral("stub"),
                         [](WorkspaceLeaf *leaf) -> View * { return new StubView(leaf); });
        reg.registerView(QStringLiteral("stub2"),
                         [](WorkspaceLeaf *leaf) -> View * { return new StubView(leaf); });
        QVERIFY(registry.getViewCreatorByType(QStringLiteral("stub")) != nullptr);
        QVERIFY(registry.getViewCreatorByType(QStringLiteral("stub2")) != nullptr);
    }
    QVERIFY(registry.getViewCreatorByType(QStringLiteral("stub")) == nullptr);
    QVERIFY(registry.getViewCreatorByType(QStringLiteral("stub2")) == nullptr);
}

void TestProxyUi::viewExtensionsCleanedOnDestroy()
{
    ViewRegistry registry;
    {
        ViewRegistrar reg(&registry);
        reg.registerExtensions({QStringLiteral("foo"), QStringLiteral("bar")},
                               QStringLiteral("stub"));
        QCOMPARE(registry.getTypeByExtension(QStringLiteral("foo")),
                 QStringLiteral("stub"));
    }
    QVERIFY(registry.getTypeByExtension(QStringLiteral("foo")).isEmpty());
    QVERIFY(registry.getTypeByExtension(QStringLiteral("bar")).isEmpty());
}

void TestProxyUi::viewHandlesNullRegistry()
{
    ViewRegistrar reg(nullptr);
    reg.registerView(QStringLiteral("x"),
                     [](WorkspaceLeaf *leaf) -> View * { return new StubView(leaf); });
    reg.registerExtensions({QStringLiteral("y")}, QStringLiteral("x"));
    reg.unregisterView(QStringLiteral("x"));
    QVERIFY(true); // no crash is the bar
}

// ---------- MenuInjector -------------------------------------------------

void TestProxyUi::fileMenuHandlerInvokedOnEmit()
{
    MenuEventEmitter emitter;
    MenuInjector inj(&emitter);

    int hits = 0;
    QString lastPath;
    inj.onFileMenuBuilt([&](QMenu *menu, const QString &path) {
        ++hits;
        lastPath = path;
        Q_UNUSED(menu);
    });

    QMenu menu;
    emitter.emitFileMenu(&menu, QStringLiteral("notes/foo.md"));
    QCOMPARE(hits, 1);
    QCOMPARE(lastPath, QStringLiteral("notes/foo.md"));
}

void TestProxyUi::editorMenuHandlerInvokedOnEmit()
{
    MenuEventEmitter emitter;
    MenuInjector inj(&emitter);

    int hits = 0;
    inj.onEditorMenuBuilt([&](QMenu *, const QString &) { ++hits; });

    QMenu menu;
    QObject ed;
    emitter.emitEditorMenu(&menu, &ed);
    QCOMPARE(hits, 1);
}

void TestProxyUi::tabMenuHandlerInvokedOnEmit()
{
    MenuEventEmitter emitter;
    MenuInjector inj(&emitter);

    int hits = 0;
    inj.onTabMenuBuilt([&](QMenu *, const QString &) { ++hits; });

    QMenu menu;
    QObject leaf;
    emitter.emitLeafMenu(&menu, &leaf);
    QCOMPARE(hits, 1);
}

void TestProxyUi::menuHandlersDisconnectedOnDestroy()
{
    MenuEventEmitter emitter;
    int hits = 0;
    {
        MenuInjector inj(&emitter);
        inj.onFileMenuBuilt([&](QMenu *, const QString &) { ++hits; });

        QMenu menu;
        emitter.emitFileMenu(&menu, QStringLiteral("a.md"));
        QCOMPARE(hits, 1);
    }
    QMenu menu;
    emitter.emitFileMenu(&menu, QStringLiteral("b.md"));
    QCOMPARE(hits, 1); // no further increment after destruction
}

void TestProxyUi::menuHandlesNullEmitter()
{
    MenuInjector inj(nullptr);
    inj.onFileMenuBuilt([](QMenu *, const QString &) {});
    inj.onEditorMenuBuilt([](QMenu *, const QString &) {});
    inj.onTabMenuBuilt([](QMenu *, const QString &) {});
    QVERIFY(true);
}

QTEST_MAIN(TestProxyUi)
#include "tst_proxy_ui.moc"
