// tests/core/tst_workspace_tree.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include <QPointer>
#include "corbomite/core/WorkspaceItem.h"
#include "corbomite/core/WorkspaceParent.h"

using Corbomite::WorkspaceItem;
using Corbomite::WorkspaceParent;

class TestItem : public WorkspaceItem
{
    Q_OBJECT
public:
    using WorkspaceItem::WorkspaceItem;
    QWidget *widget() override { return nullptr; }
    QJsonObject serialize() const override
    {
        QJsonObject json;
        json[QStringLiteral("id")] = id();
        json[QStringLiteral("type")] = QStringLiteral("test-item");
        return json;
    }
};

class TestParent : public WorkspaceParent
{
    Q_OBJECT
public:
    using WorkspaceParent::WorkspaceParent;
    QWidget *widget() override { return nullptr; }
    QJsonObject serialize() const override
    {
        QJsonObject json;
        json[QStringLiteral("id")] = id();
        json[QStringLiteral("type")] = QStringLiteral("test-parent");
        return json;
    }
};

class TestWorkspaceTree : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void itemHas16CharId()
    {
        TestItem item;
        QCOMPARE(item.id().length(), 16);
    }

    void itemIdIsUnique()
    {
        TestItem a, b;
        QVERIFY(a.id() != b.id());
    }

    void dimensionDefaultsToNull()
    {
        TestItem item;
        QVERIFY(!item.dimension().has_value());
    }

    void setDimension()
    {
        TestItem item;
        item.setDimension(50);
        QCOMPARE(item.dimension().value(), 50);
    }

    void parentChildRelationship()
    {
        TestParent parent;
        auto *child = new TestItem(&parent);
        parent.addChild(child);

        QCOMPARE(parent.childCount(), 1);
        QCOMPARE(parent.childAt(0), child);
        QCOMPARE(child->parentItem(), &parent);
    }

    void removeChild()
    {
        TestParent parent;
        auto *child = new TestItem(&parent);
        parent.addChild(child);
        parent.removeChild(child);

        QCOMPARE(parent.childCount(), 0);
        QVERIFY(child->parentItem() == nullptr);
    }

    void moveChild()
    {
        TestParent parent;
        auto *a = new TestItem(&parent);
        auto *b = new TestItem(&parent);
        auto *c = new TestItem(&parent);
        parent.addChild(a);
        parent.addChild(b);
        parent.addChild(c);

        parent.moveChild(2, 0);
        QCOMPARE(parent.childAt(0), c);
        QCOMPARE(parent.childAt(1), a);
        QCOMPARE(parent.childAt(2), b);
    }

    void insertChildAtIndex()
    {
        TestParent parent;
        auto *a = new TestItem(&parent);
        auto *b = new TestItem(&parent);
        parent.addChild(a);
        parent.addChild(b, 0);

        QCOMPARE(parent.childAt(0), b);
        QCOMPARE(parent.childAt(1), a);
    }

    void removeChildDeletesIfOwned()
    {
        TestParent parent;
        auto *child = new TestItem(&parent);
        parent.addChild(child);

        QPointer<TestItem> guard(child);
        parent.removeChild(child, true);
        QVERIFY(guard.isNull());
    }
};

QTEST_GUILESS_MAIN(TestWorkspaceTree)
#include "tst_workspace_tree.moc"
