// tests/core/tst_workspaceleaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"

using namespace Corbomite;

class LeafStubView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("leaf-stub"); }
    QString getDisplayText() const override { return QStringLiteral("Leaf Stub"); }
    QJsonObject getState() const override {
        return {{QStringLiteral("key"), QStringLiteral("value")}};
    }
};

class TestWorkspaceLeaf : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void idIsGenerated()
    {
        WorkspaceLeaf leaf(nullptr, nullptr);
        QCOMPARE(leaf.id().size(), 16);
    }

    void idsAreUnique()
    {
        WorkspaceLeaf a(nullptr, nullptr);
        WorkspaceLeaf b(nullptr, nullptr);
        QVERIFY(a.id() != b.id());
    }

    void openSetsView()
    {
        WorkspaceLeaf leaf(nullptr, nullptr);
        auto *view = new LeafStubView(&leaf);
        leaf.open(view);
        QCOMPARE(leaf.view(), view);
    }

    void serializeRoundTrip()
    {
        WorkspaceLeaf leaf(nullptr, nullptr);
        auto *view = new LeafStubView(&leaf);
        leaf.open(view);

        QJsonObject json = leaf.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("leaf"));
        QCOMPARE(json[QStringLiteral("id")].toString(), leaf.id());

        auto stateObj = json[QStringLiteral("state")].toObject();
        QCOMPARE(stateObj[QStringLiteral("type")].toString(), QStringLiteral("leaf-stub"));
    }

    void setViewStateThroughRegistry()
    {
        ViewRegistry reg;
        reg.registerView(QStringLiteral("leaf-stub"), [](WorkspaceLeaf *leaf) -> View * {
            return new LeafStubView(leaf);
        });

        WorkspaceLeaf leaf(&reg, nullptr);

        QJsonObject viewState;
        viewState[QStringLiteral("type")] = QStringLiteral("leaf-stub");
        viewState[QStringLiteral("state")] = QJsonObject{};

        leaf.setViewState(viewState);
        QVERIFY(leaf.view() != nullptr);
        QCOMPARE(leaf.view()->getViewType(), QStringLiteral("leaf-stub"));
    }

    void setViewStateUnknownTypeGivesNullView()
    {
        ViewRegistry reg;
        WorkspaceLeaf leaf(&reg, nullptr);

        QJsonObject viewState;
        viewState[QStringLiteral("type")] = QStringLiteral("nonexistent");

        leaf.setViewState(viewState);
        QVERIFY(leaf.view() == nullptr);
    }
};

QTEST_MAIN(TestWorkspaceLeaf)
#include "tst_workspaceleaf.moc"
