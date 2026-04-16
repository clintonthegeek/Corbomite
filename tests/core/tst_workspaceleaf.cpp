// tests/core/tst_workspaceleaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"
#include "corbomite/core/LeafHistory.h"

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
        WorkspaceLeaf leaf(nullptr);
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
        WorkspaceLeaf leaf(nullptr);
        auto *view = new LeafStubView(&leaf);
        leaf.open(view);
        QCOMPARE(leaf.view(), view);
    }

    void serializeRoundTrip()
    {
        WorkspaceLeaf leaf(nullptr);
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

        WorkspaceLeaf leaf(&reg);

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
        WorkspaceLeaf leaf(&reg);

        QJsonObject viewState;
        viewState[QStringLiteral("type")] = QStringLiteral("nonexistent");

        leaf.setViewState(viewState);
        QVERIFY(leaf.view() == nullptr);
    }

    // --- New tests ---

    void pinnedDefaultFalse()
    {
        WorkspaceLeaf leaf(nullptr);
        QCOMPARE(leaf.pinned(), false);
    }

    void setPinned()
    {
        WorkspaceLeaf leaf(nullptr);
        QSignalSpy spy(&leaf, &WorkspaceLeaf::pinnedChanged);

        leaf.setPinned(true);
        QCOMPARE(leaf.pinned(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);

        // Setting same value should not emit again
        leaf.setPinned(true);
        QCOMPARE(spy.count(), 1);

        leaf.setPinned(false);
        QCOMPARE(leaf.pinned(), false);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    void groupDefaultEmpty()
    {
        WorkspaceLeaf leaf(nullptr);
        QVERIFY(leaf.group().isEmpty());
    }

    void setGroup()
    {
        WorkspaceLeaf leaf(nullptr);
        QSignalSpy spy(&leaf, &WorkspaceLeaf::groupChanged);

        leaf.setGroup(QStringLiteral("red"));
        QCOMPARE(leaf.group(), QStringLiteral("red"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("red"));

        // Same value — no emit
        leaf.setGroup(QStringLiteral("red"));
        QCOMPARE(spy.count(), 1);

        leaf.setGroup(QStringLiteral("blue"));
        QCOMPARE(leaf.group(), QStringLiteral("blue"));
        QCOMPARE(spy.count(), 2);
    }

    void deferredDefaultFalse()
    {
        WorkspaceLeaf leaf(nullptr);
        QCOMPARE(leaf.isDeferred(), false);
    }

    void setDeferred()
    {
        WorkspaceLeaf leaf(nullptr);

        // Open a view, then mark as deferred
        auto *view = new LeafStubView(&leaf);
        leaf.open(view);
        QVERIFY(leaf.view() != nullptr);

        leaf.setDeferred(true, QStringLiteral("document"), QStringLiteral("MyNote.md"));
        QCOMPARE(leaf.isDeferred(), true);
        QCOMPARE(leaf.cachedIcon(), QStringLiteral("document"));
        QCOMPARE(leaf.cachedTitle(), QStringLiteral("MyNote.md"));
        // View should have been closed when deferred
        QVERIFY(leaf.view() == nullptr);

        // Clearing deferred
        leaf.setDeferred(false);
        QCOMPARE(leaf.isDeferred(), false);
    }

    void historyAccess()
    {
        WorkspaceLeaf leaf(nullptr);
        QCOMPARE(leaf.history().canGoBack(), false);
        QCOMPARE(leaf.history().canGoForward(), false);
    }

    void serializeWithNewFields()
    {
        WorkspaceLeaf leaf(nullptr);
        leaf.setPinned(true);
        leaf.setGroup(QStringLiteral("green"));

        QJsonObject json = leaf.serialize();
        QCOMPARE(json[QStringLiteral("pinned")].toBool(), true);
        QCOMPARE(json[QStringLiteral("group")].toString(), QStringLiteral("green"));
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("leaf"));
    }

    void serializeOmitsFalsePinnedAndEmptyGroup()
    {
        WorkspaceLeaf leaf(nullptr);
        // Default: pinned=false, group=""

        QJsonObject json = leaf.serialize();
        // pinned should not appear when false
        QVERIFY(!json.contains(QStringLiteral("pinned")));
        // group should not appear when empty
        QVERIFY(!json.contains(QStringLiteral("group")));
    }

    void deserializeWithNewFields()
    {
        QJsonObject json;
        json[QStringLiteral("id")] = QStringLiteral("testid1234567890");
        json[QStringLiteral("type")] = QStringLiteral("leaf");
        json[QStringLiteral("pinned")] = true;
        json[QStringLiteral("group")] = QStringLiteral("purple");
        json[QStringLiteral("state")] = QJsonObject{};

        ViewRegistry reg;
        auto *leaf = WorkspaceLeaf::deserialize(json, &reg, nullptr);

        QCOMPARE(leaf->id(), QStringLiteral("testid1234567890"));
        QCOMPARE(leaf->pinned(), true);
        QCOMPARE(leaf->group(), QStringLiteral("purple"));

        delete leaf;
    }
};

QTEST_MAIN(TestWorkspaceLeaf)
#include "tst_workspaceleaf.moc"
