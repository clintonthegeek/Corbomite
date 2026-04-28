// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QIcon>
#include <QPointer>

#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/HoverLinkSourceRegistry.h"
#include "corbomite/core/PostProcessorRegistry.h"
#include "corbomite/core/RibbonHandle.h"
#include "corbomite/core/proxies/CodeBlockRegistrar.h"
#include "corbomite/core/proxies/EditorSuggestRegistrar.h"
#include "corbomite/core/proxies/EmbedRegistrar.h"
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"
#include "corbomite/core/proxies/PostProcessorRegistrar.h"
#include "corbomite/core/proxies/RibbonRegistrar.h"

#include <markoff/CodeBlockProcessorRegistry.h>
#include <markoff/EmbedRegistry.h>
#include <markoff/MarkdownRenderChild.h>

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
};

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

QTEST_MAIN(TestProxyExtensions)
#include "tst_proxy_extensions.moc"
