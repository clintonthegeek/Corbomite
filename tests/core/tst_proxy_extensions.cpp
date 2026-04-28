// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QIcon>
#include <QPointer>

#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/HoverLinkSourceRegistry.h"
#include "corbomite/core/PostProcessorRegistry.h"
#include "corbomite/core/RibbonHandle.h"
#include "corbomite/core/StatusBarRegistry.h"
#include "corbomite/core/LucideIconRegistry.h"
#include "corbomite/core/MarkdownRenderer.h"
#include "corbomite/core/proxies/CodeBlockRegistrar.h"
#include "corbomite/core/proxies/EditorSuggestRegistrar.h"
#include "corbomite/core/proxies/EmbedRegistrar.h"
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"
#include "corbomite/core/proxies/PostProcessorRegistrar.h"
#include "corbomite/core/proxies/RibbonRegistrar.h"
#include "corbomite/core/proxies/StatusBarRegistrar.h"
#include "corbomite/core/proxies/LucideIconRegistrar.h"

#include <markoff/CodeBlockProcessorRegistry.h>
#include <markoff/EmbedRegistry.h>
#include <markoff/MarkdownRenderChild.h>

#include <QLabel>
#include <QStatusBar>

using namespace Corbomite;

namespace {

class StubSuggest : public EditorSuggest
{
public:
    std::optional<EditorSuggestTriggerInfo>
    onTrigger(int, const QString &, NoteDocument *) override { return std::nullopt; }
    QStringList getSuggestions(const EditorSuggestTriggerInfo &) override { return {}; }
    QString selectSuggestion(const QString &, const EditorSuggestTriggerInfo &) override
    { return {}; }
};

class StubRibbon : public RibbonHandle
{
public:
    QString addRibbonIcon(const QString &id, const QIcon &,
                           const QString &, std::function<void()>) override
    {
        if (m_ids.contains(id)) return {};
        m_ids.append(id);
        return id;
    }
    bool removeRibbonIcon(const QString &id) override
    {
        return m_ids.removeAll(id) > 0;
    }
    QStringList m_ids;
};

class StubRenderChild : public Markoff::MarkdownRenderChild
{
public:
    StubRenderChild() = default;
};

} // namespace

class TestProxyExtensions : public QObject
{
    Q_OBJECT

private slots:
    // HoverLinkSourceRegistrar
    void hoverPrefixesIdAndRegisters();
    void hoverDestructorRemovesAll();
    void hoverHandlesNullRegistry();

    // EditorSuggestRegistrar
    void suggestRegistersAndCleansOnDestroy();
    void suggestHandlesNullManager();

    // PostProcessorRegistrar
    void postProcessorRegistersAndCleansOnDestroy();
    void postProcessorHandlesNullRegistry();

    // RibbonRegistrar
    void ribbonNamespacesIds();
    void ribbonDestructorRemovesAll();

    // EmbedRegistrar
    void embedRegistersAndCleansOnDestroy();
    void embedFirstWinsCollision();

    // CodeBlockRegistrar
    void codeBlockRegistersAndCleansOnDestroy();
    void codeBlockFirstWinsCollision();

    // StatusBarRegistrar
    void statusBarPrefixesIdAndAdds();
    void statusBarDestructorRemovesAll();
    void statusBarHandlesNullRegistry();

    // LucideIconRegistry / LucideIconRegistrar
    void lucideAddIconStoresUnderName();
    void lucideRegistrarPrefixesAndCleansOnDestroy();
    void lucideRejectsInvalidSvg();

    // MarkdownRenderer::render
    void renderAttachesReadingViewAndCompletesFuture();
    void renderHandlesNullParent();
};

namespace {
constexpr auto kSampleSvg =
    R"(<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg" )"
    R"(viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">)"
    R"(<circle cx="12" cy="12" r="10"/></svg>)";
} // namespace

// ---- HoverLinkSourceRegistrar ----

void TestProxyExtensions::hoverPrefixesIdAndRegisters()
{
    HoverLinkSourceRegistry registry;
    HoverLinkSourceRegistrar reg(&registry, QStringLiteral("plug-a"));

    HoverLinkSource src;
    src.id = QStringLiteral("backlinks");
    QVERIFY(reg.registerSource(src));
    QCOMPARE(src.id, QStringLiteral("plug-a:backlinks"));
    QVERIFY(registry.isRegistered(QStringLiteral("plug-a:backlinks")));
}

void TestProxyExtensions::hoverDestructorRemovesAll()
{
    HoverLinkSourceRegistry registry;
    {
        HoverLinkSourceRegistrar reg(&registry, QStringLiteral("plug-a"));
        HoverLinkSource a; a.id = QStringLiteral("a");
        HoverLinkSource b; b.id = QStringLiteral("b");
        reg.registerSource(a);
        reg.registerSource(b);
    }
    QVERIFY(!registry.isRegistered(QStringLiteral("plug-a:a")));
    QVERIFY(!registry.isRegistered(QStringLiteral("plug-a:b")));
}

void TestProxyExtensions::hoverHandlesNullRegistry()
{
    HoverLinkSourceRegistrar reg(nullptr, QStringLiteral("p"));
    HoverLinkSource src;
    src.id = QStringLiteral("x");
    QVERIFY(!reg.registerSource(src));
    reg.unregisterSource(QStringLiteral("x")); // does not crash
}

// ---- EditorSuggestRegistrar ----

void TestProxyExtensions::suggestRegistersAndCleansOnDestroy()
{
    EditorSuggestManager mgr;
    StubSuggest s1, s2;
    {
        EditorSuggestRegistrar reg(&mgr);
        reg.registerSuggest(&s1);
        reg.registerSuggest(&s2);
        QCOMPARE(mgr.suggesterCount(), 2);
    }
    QCOMPARE(mgr.suggesterCount(), 0);
}

void TestProxyExtensions::suggestHandlesNullManager()
{
    EditorSuggestRegistrar reg(nullptr);
    StubSuggest s;
    reg.registerSuggest(&s); // no crash
    reg.unregisterSuggest(&s);
}

// ---- PostProcessorRegistrar ----

void TestProxyExtensions::postProcessorRegistersAndCleansOnDestroy()
{
    Corbomite::Core::PostProcessorRegistry registry;
    int calls = 0;
    {
        PostProcessorRegistrar reg(&registry);
        auto h = reg.registerProcessor(0,
            [&calls](void *, const Corbomite::Core::PostProcessorContext &) {
                ++calls;
            });
        QVERIFY(h.id != 0);
        registry.run(nullptr, {});
        QCOMPARE(calls, 1);
    }
    registry.run(nullptr, {});
    QCOMPARE(calls, 1); // destructor unregistered
}

void TestProxyExtensions::postProcessorHandlesNullRegistry()
{
    PostProcessorRegistrar reg(nullptr);
    auto h = reg.registerProcessor(0, [](void *, const auto &) {});
    QCOMPARE(h.id, 0u);
}

// ---- RibbonRegistrar ----

void TestProxyExtensions::ribbonNamespacesIds()
{
    StubRibbon ribbon;
    RibbonRegistrar reg(&ribbon, QStringLiteral("plug-a"));
    const QString id = reg.addRibbonIcon(QStringLiteral("hello"), QIcon(),
                                            QStringLiteral("Hello"), [] {});
    QCOMPARE(id, QStringLiteral("plug-a:hello"));
    QVERIFY(ribbon.m_ids.contains(QStringLiteral("plug-a:hello")));
}

void TestProxyExtensions::ribbonDestructorRemovesAll()
{
    StubRibbon ribbon;
    {
        RibbonRegistrar reg(&ribbon, QStringLiteral("plug-a"));
        reg.addRibbonIcon(QStringLiteral("a"), QIcon(), QStringLiteral("A"), [] {});
        reg.addRibbonIcon(QStringLiteral("b"), QIcon(), QStringLiteral("B"), [] {});
        QCOMPARE(ribbon.m_ids.size(), 2);
    }
    QCOMPARE(ribbon.m_ids.size(), 0);
}

// ---- EmbedRegistrar ----

void TestProxyExtensions::embedRegistersAndCleansOnDestroy()
{
    Markoff::EmbedRegistry registry;
    {
        EmbedRegistrar reg(&registry);
        QVERIFY(reg.registerExtension(QStringLiteral("foo"),
                                          [](const Markoff::EmbedRequest &) {
                                              return std::unique_ptr<Markoff::MarkdownRenderChild>(
                                                  new StubRenderChild());
                                          }));
        QVERIFY(registry.hasExtension(QStringLiteral("foo")));
    }
    QVERIFY(!registry.hasExtension(QStringLiteral("foo")));
}

void TestProxyExtensions::embedFirstWinsCollision()
{
    Markoff::EmbedRegistry registry;
    registry.registerExtension(QStringLiteral("png"),
        [](const Markoff::EmbedRequest &) {
            return std::unique_ptr<Markoff::MarkdownRenderChild>(new StubRenderChild());
        });
    EmbedRegistrar reg(&registry);
    QVERIFY(!reg.registerExtension(QStringLiteral("png"),
        [](const Markoff::EmbedRequest &) {
            return std::unique_ptr<Markoff::MarkdownRenderChild>(new StubRenderChild());
        }));
}

// ---- CodeBlockRegistrar ----

void TestProxyExtensions::codeBlockRegistersAndCleansOnDestroy()
{
    Markoff::CodeBlockProcessorRegistry registry;
    {
        CodeBlockRegistrar reg(&registry);
        QVERIFY(reg.registerLanguage(QStringLiteral("foo"),
            [](const QString &, void *, const Markoff::CodeBlockContext &) {
                return true;
            }));
        QVERIFY(registry.hasLanguage(QStringLiteral("foo")));
    }
    QVERIFY(!registry.hasLanguage(QStringLiteral("foo")));
}

void TestProxyExtensions::codeBlockFirstWinsCollision()
{
    Markoff::CodeBlockProcessorRegistry registry;
    registry.registerLanguage(QStringLiteral("mermaid"),
        [](const QString &, void *, const Markoff::CodeBlockContext &) { return true; });
    CodeBlockRegistrar reg(&registry);
    QVERIFY(!reg.registerLanguage(QStringLiteral("mermaid"),
        [](const QString &, void *, const Markoff::CodeBlockContext &) { return true; }));
}

// ---- StatusBarRegistrar ----

void TestProxyExtensions::statusBarPrefixesIdAndAdds()
{
    QStatusBar bar;
    StatusBarRegistry registry(&bar);
    StatusBarRegistrar reg(&registry, QStringLiteral("plug-a"));
    auto *label = new QLabel(QStringLiteral("hi"));
    const QString id = reg.addItem(QStringLiteral("info"), label);
    QCOMPARE(id, QStringLiteral("plug-a:info"));
    QVERIFY(registry.hasItem(QStringLiteral("plug-a:info")));
}

void TestProxyExtensions::statusBarDestructorRemovesAll()
{
    QStatusBar bar;
    StatusBarRegistry registry(&bar);
    {
        StatusBarRegistrar reg(&registry, QStringLiteral("plug-a"));
        reg.addItem(QStringLiteral("a"), new QLabel());
        reg.addItem(QStringLiteral("b"), new QLabel());
        QCOMPARE(registry.itemCount(), 2);
    }
    QCOMPARE(registry.itemCount(), 0);
}

void TestProxyExtensions::statusBarHandlesNullRegistry()
{
    StatusBarRegistrar reg(nullptr, QStringLiteral("p"));
    auto *label = new QLabel();
    QVERIFY(reg.addItem(QStringLiteral("x"), label).isEmpty());
    delete label; // we still own it because addItem refused
}

// ---- LucideIconRegistry / LucideIconRegistrar ----

void TestProxyExtensions::lucideAddIconStoresUnderName()
{
    auto &r = LucideIconRegistry::instance();
    r.clearForTesting();
    r.addIcon(QStringLiteral("circle"), QByteArray(kSampleSvg));
    QVERIFY(r.hasIcon(QStringLiteral("circle")));
    QVERIFY(!r.get(QStringLiteral("circle")).isNull());
    r.clearForTesting();
}

void TestProxyExtensions::lucideRegistrarPrefixesAndCleansOnDestroy()
{
    auto &r = LucideIconRegistry::instance();
    r.clearForTesting();
    {
        LucideIconRegistrar reg(&r, QStringLiteral("plug-a"));
        const QString name = reg.addIcon(QStringLiteral("circle"),
                                            QByteArray(kSampleSvg));
        QCOMPARE(name, QStringLiteral("plug-a:circle"));
        QVERIFY(r.hasIcon(QStringLiteral("plug-a:circle")));
    }
    QVERIFY(!r.hasIcon(QStringLiteral("plug-a:circle")));
    r.clearForTesting();
}

void TestProxyExtensions::lucideRejectsInvalidSvg()
{
    auto &r = LucideIconRegistry::instance();
    r.clearForTesting();
    LucideIconRegistrar reg(&r, QStringLiteral("plug-a"));
    QVERIFY(reg.addIcon(QStringLiteral("bad"), QByteArray("not valid svg")).isEmpty());
    QVERIFY(!r.hasIcon(QStringLiteral("plug-a:bad")));
    r.clearForTesting();
}

// ---- MarkdownRenderer::render ----

void TestProxyExtensions::renderAttachesReadingViewAndCompletesFuture()
{
    QWidget parent;
    QObject lifetime;
    auto fut = MarkdownRenderer::render(QStringLiteral("# Hi\n\nBody."),
                                          &parent,
                                          QStringLiteral("test.md"),
                                          &lifetime);
    QVERIFY(fut.isFinished());
    QVERIFY(parent.findChildren<QWidget *>().size() > 0);
}

void TestProxyExtensions::renderHandlesNullParent()
{
    QObject lifetime;
    auto fut = MarkdownRenderer::render(QStringLiteral("# Hi"),
                                          nullptr,
                                          QString(),
                                          &lifetime);
    QVERIFY(fut.isFinished());
}

QTEST_MAIN(TestProxyExtensions)
#include "tst_proxy_extensions.moc"
