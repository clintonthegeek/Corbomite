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

/// Markdown-typed stub that round-trips both `state` and `ephemeralState`.
/// Cluster Y Phase 7.4 tests need a leaf whose `view->getViewType()`
/// actually reports "markdown" (StubView lies and reports "stub" even
/// when registered under "markdown") and whose ephemeral state survives
/// `setEphemeralState` (the View base no-ops both accessors).
class MarkdownStubView : public View
{
    Q_OBJECT
public:
    explicit MarkdownStubView(WorkspaceLeaf *leaf, QWidget *parent = nullptr)
        : View(leaf, parent) {}
    QString getViewType() const override { return QStringLiteral("markdown"); }
    QString getDisplayText() const override { return QStringLiteral("Markdown Stub"); }
    QJsonObject getState() const override { return m_state; }
    void setState(const QJsonObject &state) override { m_state = state; }
    QJsonObject getEphemeralState() const override { return m_eState; }
    void setEphemeralState(const QJsonObject &state) override { m_eState = state; }

private:
    QJsonObject m_state;
    QJsonObject m_eState;
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
        [](WorkspaceLeaf *leaf) -> View * { return new MarkdownStubView(leaf); });
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

    // Cluster Y Phase 7.4 — Obsidian-shape additions.
    void getLeavesOfType_returnsMatchingLeafIds();
    void iterateAllLeaves_visitsEachLeafOnce();
    void getActiveViewOfType_matchesOnlyWhenTypeAgrees();
    void openLinkText_stringMode_dispatchesToWorkspace();
    void getLeaf_stringMode_returnsLeafId();
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

void TestProxyWorkspace::getLeavesOfType_returnsMatchingLeafIds()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("a.md")));
    const QString aId = ctrl.activeLeafId();
    QVERIFY(ctrl.openFile(QStringLiteral("b.md")));
    const QString bId = ctrl.activeLeafId();

    auto *otherLeaf = workspace.createLeafInActiveGroup();
    QVERIFY(otherLeaf);
    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("stub");
    otherLeaf->setViewState(viewState);
    const QString otherId = otherLeaf->id();

    const QStringList markdown = ctrl.getLeavesOfType(QStringLiteral("markdown"));
    QCOMPARE(markdown.size(), 2);
    QVERIFY(markdown.contains(aId));
    QVERIFY(markdown.contains(bId));
    QVERIFY(!markdown.contains(otherId));

    QCOMPARE(ctrl.getLeavesOfType(QStringLiteral("stub")).size(), 1);
    QVERIFY(ctrl.getLeavesOfType(QStringLiteral("nope")).isEmpty());
}

void TestProxyWorkspace::iterateAllLeaves_visitsEachLeafOnce()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("a.md")));
    QVERIFY(ctrl.openFile(QStringLiteral("b.md")));

    QStringList visited;
    ctrl.iterateAllLeaves([&visited](const QString &id) { visited.append(id); });
    QCOMPARE(visited.size(), 2);

    QStringList expected;
    for (auto *leaf : workspace.allLeaves())
        expected.append(leaf->id());
    QCOMPARE(visited, expected);
}

void TestProxyWorkspace::getActiveViewOfType_matchesOnlyWhenTypeAgrees()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("a.md")));
    const QString id = ctrl.activeLeafId();
    QCOMPARE(ctrl.getActiveViewOfType(QStringLiteral("markdown")), id);
    QVERIFY(ctrl.getActiveViewOfType(QStringLiteral("stub")).isEmpty());
    QVERIFY(ctrl.getActiveViewOfType(QString{}).isEmpty());
}

void TestProxyWorkspace::openLinkText_stringMode_dispatchesToWorkspace()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openLinkText(QStringLiteral("Note#H"), QString{},
                               QStringLiteral("tab")));

    auto *leaf = workspace.activeLeaf();
    QVERIFY(leaf);
    QCOMPARE(leaf->getViewState()
                  .value(QStringLiteral("state")).toObject()
                  .value(QStringLiteral("file")).toString(),
              QStringLiteral("Note"));
    QCOMPARE(leaf->getEphemeralState()
                  .value(QStringLiteral("subpath")).toString(),
              QStringLiteral("#H"));
}

void TestProxyWorkspace::getLeaf_stringMode_returnsLeafId()
{
    auto *registry = makeRegistry(this);
    Workspace workspace(registry);
    WorkspaceController ctrl(&workspace);

    QVERIFY(ctrl.openFile(QStringLiteral("a.md")));
    const QString seedId = ctrl.activeLeafId();

    const QString tabId = ctrl.getLeaf(QStringLiteral("tab"));
    QVERIFY(!tabId.isEmpty());
    QVERIFY(tabId != seedId);

    const QString splitId = ctrl.getLeaf(QStringLiteral("split"),
                                          QStringLiteral("vertical"));
    QVERIFY(!splitId.isEmpty());
    QVERIFY(splitId != tabId);

    QVERIFY(ctrl.getLeaf(QStringLiteral("nonsense")).isEmpty()
            == false);  // unrecognised mode falls back to "tab".
}

QTEST_MAIN(TestProxyWorkspace)
#include "tst_proxy_workspace.moc"
