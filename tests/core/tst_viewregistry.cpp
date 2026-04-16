// tests/core/tst_viewregistry.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"

using namespace Corbomite;

class RegistryStubView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
};

class TestViewRegistry : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void registerAndLookup()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("test"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("test")) != nullptr);
    }

    void duplicateRegistrationThrows()
    {
        ViewRegistry reg;
        auto factory = [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); };
        reg.registerView(QStringLiteral("test"), factory);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            reg.registerView(QStringLiteral("test"), factory));
    }

    void unregisterRemovesType()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("test"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        reg.unregisterView(QStringLiteral("test"));
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("test")) == nullptr);
    }

    void unregisterAbsentIsNoOp()
    {
        ViewRegistry reg;
        reg.unregisterView(QStringLiteral("nonexistent"));
    }

    void extensionRegistration()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("markdown"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("markdown"));
        QCOMPARE(reg.getTypeByExtension(QStringLiteral("md")), QStringLiteral("markdown"));
        QVERIFY(reg.isExtensionRegistered(QStringLiteral("md")));
    }

    void duplicateExtensionThrows()
    {
        ViewRegistry reg;
        auto factory = [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); };
        reg.registerView(QStringLiteral("t1"), factory);
        reg.registerView(QStringLiteral("t2"), factory);
        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("t1"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("t2")));
    }

    void atomicExtensionRegistration()
    {
        ViewRegistry reg;
        auto factory = [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); };
        reg.registerView(QStringLiteral("t1"), factory);
        reg.registerView(QStringLiteral("t2"), factory);
        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("t1"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
            reg.registerExtensions({QStringLiteral("txt"), QStringLiteral("md")}, QStringLiteral("t2")));
        QVERIFY(!reg.isExtensionRegistered(QStringLiteral("txt")));
    }

    void registerViewWithExtensions()
    {
        ViewRegistry reg;
        reg.registerViewWithExtensions(
            {QStringLiteral("md")}, QStringLiteral("markdown"),
            [](WorkspaceLeaf *leaf) -> View * { return new RegistryStubView(leaf); });
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("markdown")) != nullptr);
        QCOMPARE(reg.getTypeByExtension(QStringLiteral("md")), QStringLiteral("markdown"));
    }

    void signalEmission()
    {
        ViewRegistry reg;
        QSignalSpy regSpy(&reg, &ViewRegistry::viewRegistered);
        QSignalSpy unregSpy(&reg, &ViewRegistry::viewUnregistered);
        QSignalSpy extSpy(&reg, &ViewRegistry::extensionsUpdated);

        reg.registerView(QStringLiteral("test"), [](WorkspaceLeaf *leaf) -> View * {
            return new RegistryStubView(leaf);
        });
        QCOMPARE(regSpy.count(), 1);

        reg.registerExtensions({QStringLiteral("md")}, QStringLiteral("test"));
        QCOMPARE(extSpy.count(), 1);

        reg.unregisterView(QStringLiteral("test"));
        QCOMPARE(unregSpy.count(), 1);
    }

    void unknownTypeLookupReturnsNull()
    {
        ViewRegistry reg;
        QVERIFY(reg.getViewCreatorByType(QStringLiteral("nonexistent")) == nullptr);
    }

    void unknownExtLookupReturnsEmpty()
    {
        ViewRegistry reg;
        QVERIFY(reg.getTypeByExtension(QStringLiteral("xyz")).isEmpty());
    }
};

QTEST_MAIN(TestViewRegistry)
#include "tst_viewregistry.moc"
