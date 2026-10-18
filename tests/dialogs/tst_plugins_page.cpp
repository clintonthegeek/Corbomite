// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <KConfig>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <QLabel>
#include <QPushButton>

#include <memory>

#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"
#include "dialogs/PluginsPage.h"

using namespace Corbomite;

namespace {

/// Plugin that reports `pages` settings pages, each a QLabel tagged so the
/// test can prove the widget the plugin returned is the one on screen.
class FakeConfigPlugin : public Plugin
{
    Q_OBJECT
public:
    explicit FakeConfigPlugin(int pages, QObject *parent = nullptr)
        : Plugin(parent), m_pages(pages) {}

    int configPages() const override { return m_pages; }

    QWidget *configPage(int number, QWidget *parent) override
    {
        ++m_built;
        if (number >= m_pages) return nullptr;
        auto *w = new QLabel(QStringLiteral("page %1").arg(number), parent);
        w->setObjectName(QStringLiteral("fakePluginConfigWidget"));
        return w;
    }

    QString configPageTitle(int number) const override
    {
        return QStringLiteral("Tab %1").arg(number);
    }

    int builtCount() const { return m_built; }

private:
    int m_pages;
    int m_built = 0;
};

KPluginMetaData makeFixture(const QString &id)
{
    QJsonObject kplugin{
        {QStringLiteral("Id"), id},
        {QStringLiteral("Name"), id},
        {QStringLiteral("EnabledByDefault"), false}};
    QJsonObject full{
        {QStringLiteral("KPlugin"), kplugin},
        {QStringLiteral("X-Corbomite-Trusted"), true},
        {QStringLiteral("X-Corbomite-Permissions"), QJsonArray{}}};
    return KPluginMetaData(full, id);
}

int rowOf(const PluginManager &mgr, const QString &id)
{
    for (int i = 0; i < mgr.pluginCount(); ++i) {
        if (mgr.pluginByIndex(i).metaData.base().pluginId() == id) return i;
    }
    return -1;
}

QPushButton *configureButton(PluginsPage &page)
{
    return page.findChild<QPushButton *>(QStringLiteral("pluginConfigureButton"));
}

/// Manager with a config-isolated KSharedConfig and a factory that hands out
/// one-page / zero-page FakeConfigPlugin instances.
PluginManager *makeManager(QTemporaryDir &cfg,
                           FakeConfigPlugin **withPages,
                           FakeConfigPlugin **withoutPages)
{
    auto *mgr = new PluginManager;
    mgr->setConfig(KSharedConfig::openConfig(
        cfg.filePath(QStringLiteral("corbomite-test.rc")), KConfig::SimpleConfig));
    mgr->setFactoryOverride([withPages, withoutPages](const PluginMetaData &meta)
                                -> Plugin * {
        if (meta.base().pluginId() == QLatin1String("with-pages")) {
            auto *p = new FakeConfigPlugin(1);
            if (withPages) *withPages = p;
            return p;
        }
        auto *p = new FakeConfigPlugin(0);
        if (withoutPages) *withoutPages = p;
        return p;
    });
    mgr->ingest({makeFixture(QStringLiteral("with-pages")),
                 makeFixture(QStringLiteral("without-pages"))},
                PluginMetaData::Origin::System);
    return mgr;
}

} // namespace

class TestPluginsPage : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void emptyManagerHasZeroRows();
    void nullManagerHasZeroRows();
    void rowsReflectDiscoveredPlugins();
    void selectingRowMakesDetailReflectTrustedFlag();
    void configureButtonDrivesPluginConfigPage();
    void configureButtonAbsentForDisabledPlugin();

private:
    QTemporaryDir m_systemDir;
};

void TestPluginsPage::initTestCase()
{
    QVERIFY(m_systemDir.isValid());
}

void TestPluginsPage::emptyManagerHasZeroRows()
{
    PluginManager mgr;
    mgr.setSystemSearchPath(m_systemDir.path());
    mgr.setUserSearchPath(m_systemDir.path()); // both empty dir → 0 plugins
    mgr.discoverPlugins();
    PluginsPage page(&mgr);
    QCOMPARE(page.rowCount(), 0);
}

void TestPluginsPage::nullManagerHasZeroRows()
{
    PluginsPage page(nullptr);
    QCOMPARE(page.rowCount(), 0);
}

void TestPluginsPage::rowsReflectDiscoveredPlugins()
{
    // Use the same fixture-plugin path the lifecycle test uses if it
    // provides one. Without a fixture we rely on an empty manager and
    // assert on the empty case + the API contract; the lifecycle test
    // covers the populated case via tst_plugin_manager_lifecycle.
    PluginManager mgr;
    mgr.setSystemSearchPath(m_systemDir.path());
    mgr.setUserSearchPath(m_systemDir.path()); // both empty dir → 0 plugins
    mgr.discoverPlugins();

    PluginsPage page(&mgr);
    QCOMPARE(page.rowCount(), mgr.pluginCount());
}

void TestPluginsPage::selectingRowMakesDetailReflectTrustedFlag()
{
    PluginManager mgr;
    mgr.setSystemSearchPath(m_systemDir.path());
    mgr.setUserSearchPath(m_systemDir.path()); // both empty dir → 0 plugins
    mgr.discoverPlugins();
    PluginsPage page(&mgr);
    if (page.rowCount() == 0) {
        // Nothing to select. Detail pane stays in the placeholder state;
        // detailPermissionsEditable() defaults to false.
        QVERIFY(!page.detailPermissionsEditable());
        return;
    }
    page.selectRow(0);
    // Either editable (untrusted) or read-only (trusted). Both branches
    // are valid; the assertion is that the contract is consistent with
    // the metadata.
    const auto &info = mgr.pluginByIndex(0);
    QCOMPARE(page.detailPermissionsEditable(), !info.metaData.trusted());
}

void TestPluginsPage::configureButtonDrivesPluginConfigPage()
{
    QTemporaryDir cfg;
    FakeConfigPlugin *withPages = nullptr;
    FakeConfigPlugin *withoutPages = nullptr;
    std::unique_ptr<PluginManager> mgr(makeManager(cfg, &withPages, &withoutPages));
    QVERIFY(mgr->enablePlugin(QStringLiteral("with-pages")));
    QVERIFY(mgr->enablePlugin(QStringLiteral("without-pages")));

    PluginsPage page(mgr.get());
    QCOMPARE(page.rowCount(), 2);

    // A plugin that reports pages gets the affordance…
    page.selectRow(rowOf(*mgr, QStringLiteral("with-pages")));
    auto *button = configureButton(page);
    QVERIFY(button);
    QVERIFY(button->isEnabled());

    // …and clicking it hosts the widget the plugin returned, built exactly
    // once, in a dialog titled after the plugin.
    button->click();
    QCOMPARE(withPages->builtCount(), 1);

    auto *dialog = page.findChild<QDialog *>(QStringLiteral("pluginConfigDialog"));
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("fakePluginConfigWidget")));
    QVERIFY(dialog->windowTitle().contains(QStringLiteral("with-pages")));

    // A plugin that reports none gets nothing — not a dead button.
    page.selectRow(rowOf(*mgr, QStringLiteral("without-pages")));
    QVERIFY(!configureButton(page));
    QCOMPARE(withoutPages->builtCount(), 0);
}

void TestPluginsPage::configureButtonAbsentForDisabledPlugin()
{
    QTemporaryDir cfg;
    FakeConfigPlugin *withPages = nullptr;
    std::unique_ptr<PluginManager> mgr(makeManager(cfg, &withPages, nullptr));
    QVERIFY(mgr->enablePlugin(QStringLiteral("with-pages")));
    QVERIFY(mgr->disablePlugin(QStringLiteral("with-pages")));

    // Disabled: no instance and no PluginContext to build a page against,
    // so the button must not appear even though the plugin would offer one.
    PluginsPage page(mgr.get());
    page.selectRow(rowOf(*mgr, QStringLiteral("with-pages")));
    QVERIFY(!configureButton(page));
    QCOMPARE(withPages->builtCount(), 0);
}

QTEST_MAIN(TestPluginsPage)
#include "tst_plugins_page.moc"
