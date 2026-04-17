// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include <QSignalSpy>

#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/proxies/WorkspaceController.h"

using namespace Corbomite;

namespace {

class StubView : public View
{
    Q_OBJECT
public:
    explicit StubView(WorkspaceLeaf *leaf, QWidget *parent = nullptr)
        : View(leaf, parent) {}
    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
    QJsonObject getState() const override { return m_state; }
    void setState(const QJsonObject &state) override { m_state = state; }

private:
    QJsonObject m_state;
};

ViewRegistry *makeRegistry(QObject *parent)
{
    auto *registry = new ViewRegistry(parent);
    registry->registerView(
        QStringLiteral("stub"),
        [](WorkspaceLeaf *leaf) -> View * { return new StubView(leaf); });
    registry->registerView(
        QStringLiteral("markdown"),
        [](WorkspaceLeaf *leaf) -> View * { return new StubView(leaf); });
    registry->registerExtensions({QStringLiteral("md")},
                                 QStringLiteral("markdown"));
    return registry;
}

} // namespace

class TestProxyWorkspace : public QObject
{
    Q_OBJECT
private slots:
    void nullWorkspaceReturnsSafeDefaults();
    void openFileCreatesLeafAndActivates();
    void openFileReusesExistingLeafForSamePath();
    void activeLeafIdReflectsWorkspaceState();
    void splitLeafReturnsTrueForKnownLeaf();
    void splitLeafReturnsFalseForUnknownLeaf();
    void closeLeafRemovesLeaf();
    void closeLeafReturnsFalseForUnknownLeaf();
};

void TestProxyWorkspace::nullWorkspaceReturnsSafeDefaults()
{
    WorkspaceController ctrl(nullptr);
    QVERIFY(!ctrl.openFile(QStringLiteral("a.md")));
    QVERIFY(ctrl.activeLeafId().isEmpty());
    QVERIFY(!ctrl.splitLeaf(QStringLiteral("x"), Qt::Horizontal));
    QVERIFY(!ctrl.closeLeaf(QStringLiteral("x")));
    QVERIFY(!ctrl.popoutLeaf(QStringLiteral("x")));
}

void TestProxyWorkspace::openFileCreatesLeafAndActivates()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    const int before = workspace.allLeaves().size();
    QVERIFY(ctrl.openFile(QStringLiteral("note.md")));
    QCOMPARE(workspace.allLeaves().size(), before + 1);
    QVERIFY(workspace.activeLeaf() != nullptr);
}

void TestProxyWorkspace::openFileReusesExistingLeafForSamePath()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("note.md")));
    const int afterFirst = workspace.allLeaves().size();
    QVERIFY(ctrl.openFile(QStringLiteral("note.md")));
    QCOMPARE(workspace.allLeaves().size(), afterFirst);
}

void TestProxyWorkspace::activeLeafIdReflectsWorkspaceState()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("note.md")));
    const QString id = ctrl.activeLeafId();
    QVERIFY(!id.isEmpty());
    QCOMPARE(workspace.activeLeaf()->id(), id);
}

void TestProxyWorkspace::splitLeafReturnsTrueForKnownLeaf()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("note.md")));
    const QString leafId = ctrl.activeLeafId();
    QSignalSpy layoutSpy(&workspace, &Workspace::layoutChanged);
    QVERIFY(ctrl.splitLeaf(leafId, Qt::Horizontal));
    // Workspace::splitLeaf emits layoutChanged and inserts a new empty
    // Tabs alongside the existing one; leaf count stays constant.
    QVERIFY(layoutSpy.count() >= 1);
}

void TestProxyWorkspace::splitLeafReturnsFalseForUnknownLeaf()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);
    QVERIFY(!ctrl.splitLeaf(QStringLiteral("not-a-real-id"), Qt::Horizontal));
}

void TestProxyWorkspace::closeLeafRemovesLeaf()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("a.md")));
    QVERIFY(ctrl.openFile(QStringLiteral("b.md")));
    const QString leafId = ctrl.activeLeafId();
    const int before = workspace.allLeaves().size();

    QVERIFY(ctrl.closeLeaf(leafId));
    QCOMPARE(workspace.allLeaves().size(), before - 1);
}

void TestProxyWorkspace::closeLeafReturnsFalseForUnknownLeaf()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);
    QVERIFY(!ctrl.closeLeaf(QStringLiteral("not-a-real-id")));
}

QTEST_MAIN(TestProxyWorkspace)
#include "tst_proxy_workspace.moc"
