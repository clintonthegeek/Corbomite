// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include <QSignalSpy>

#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
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

/// EditableFileView subclass that records goToLine calls so the goToLine
/// proxy wiring can be verified without a real Markdown editor.
class StubEditable : public EditableFileView
{
    Q_OBJECT
public:
    explicit StubEditable(WorkspaceLeaf *leaf, QWidget *parent = nullptr)
        : EditableFileView(leaf, parent) {}
    QString getViewType() const override { return QStringLiteral("stub-editable"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
    QJsonObject getState() const override { return {}; }
    void setState(const QJsonObject &) override {}

    bool setCursorLine(int line) override
    {
        lastLine = line;
        return true;
    }

    int lastLine = -1;
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
    registry->registerView(
        QStringLiteral("stub-editable"),
        [](WorkspaceLeaf *leaf) -> View * { return new StubEditable(leaf); });
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
    void goToLineReturnsFalseWithoutActiveEditable();
    void goToLineDispatchesToActiveEditableFileView();
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

void TestProxyWorkspace::goToLineReturnsFalseWithoutActiveEditable()
{
    // No workspace → false.
    {
        WorkspaceController ctrl(nullptr);
        QVERIFY(!ctrl.goToLine(5));
    }
    // Workspace with a non-editable active view → false.
    {
        auto *registry = makeRegistry(this);
        Workspace workspace(registry);
        WorkspaceController ctrl(&workspace);
        QVERIFY(ctrl.openFile(QStringLiteral("note.md")));
        QVERIFY(!ctrl.goToLine(5));
    }
}

void TestProxyWorkspace::goToLineDispatchesToActiveEditableFileView()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    auto *leaf = workspace.createLeafInActiveGroup();
    QVERIFY(leaf);
    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("stub-editable");
    leaf->setViewState(viewState);
    workspace.setActiveLeaf(leaf);

    auto *stub = qobject_cast<StubEditable *>(leaf->view());
    QVERIFY(stub);
    QCOMPARE(stub->lastLine, -1);
    QVERIFY(ctrl.goToLine(42));
    QCOMPARE(stub->lastLine, 42);
}

QTEST_MAIN(TestProxyWorkspace)
#include "tst_proxy_workspace.moc"
