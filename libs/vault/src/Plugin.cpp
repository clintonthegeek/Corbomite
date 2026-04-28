// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/Plugin.h"

#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/proxies/CodeBlockRegistrar.h"
#include "corbomite/core/proxies/EditorSuggestRegistrar.h"
#include "corbomite/core/proxies/EmbedRegistrar.h"
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"
#include "corbomite/core/proxies/PostProcessorRegistrar.h"
#include "corbomite/core/proxies/RibbonRegistrar.h"
#include "corbomite/vault/PluginContext.h"

#include <QJsonObject>
#include <QWidget>

namespace Corbomite {

Plugin::Plugin(QObject *parent) : QObject(parent), Component() {}
Plugin::~Plugin() = default;

void Plugin::load(PluginContext *ctx)
{
    if (isLoaded()) return;
    m_context = ctx;
    Component::load(); // fires onload() → onLoad(m_context), then children
}

void Plugin::onload()
{
    onLoad(m_context);
}

void Plugin::onunload()
{
    onUnload();
    m_context = nullptr;
}

QObject *Plugin::createView(MainWindow *)
{
    return nullptr;
}

void Plugin::focus(QObject *view)
{
    if (auto *w = qobject_cast<QWidget *>(view)) {
        w->setFocus();
    }
}

QJsonObject Plugin::saveSessionState(QObject *) const
{
    return {};
}

void Plugin::loadSessionState(QObject *, const QJsonObject &)
{
}

KTextEditor::ConfigPage *Plugin::configPage(int, QWidget *)
{
    return nullptr;
}

// ---- Cluster B Phase 1 — plugin extension verb facades -----------------

bool Plugin::registerHoverLinkSource(HoverLinkSource &source)
{
    if (!m_context) return false;
    auto *r = m_context->hoverLinkSources();
    return r ? r->registerSource(source) : false;
}

void Plugin::unregisterHoverLinkSource(const QString &localId)
{
    if (!m_context) return;
    if (auto *r = m_context->hoverLinkSources()) r->unregisterSource(localId);
}

void Plugin::registerEditorSuggest(EditorSuggest *suggester)
{
    if (!m_context) return;
    if (auto *r = m_context->editorSuggests()) r->registerSuggest(suggester);
}

void Plugin::unregisterEditorSuggest(EditorSuggest *suggester)
{
    if (!m_context) return;
    if (auto *r = m_context->editorSuggests()) r->unregisterSuggest(suggester);
}

Corbomite::Core::PostProcessorRegistry::Handle
Plugin::registerMarkdownPostProcessor(int priority,
                                        Corbomite::Core::PostProcessorFn fn)
{
    if (!m_context) return {};
    auto *r = m_context->postProcessors();
    return r ? r->registerProcessor(priority, std::move(fn))
             : Corbomite::Core::PostProcessorRegistry::Handle{};
}

void Plugin::unregisterMarkdownPostProcessor(
    Corbomite::Core::PostProcessorRegistry::Handle handle)
{
    if (!m_context) return;
    if (auto *r = m_context->postProcessors()) r->unregister(handle);
}

QString Plugin::addRibbonIcon(const QString &localId,
                                const QIcon &icon,
                                const QString &title,
                                std::function<void()> onActivated)
{
    if (!m_context) return {};
    auto *r = m_context->ribbon();
    return r ? r->addRibbonIcon(localId, icon, title, std::move(onActivated))
             : QString{};
}

bool Plugin::removeRibbonIcon(const QString &localId)
{
    if (!m_context) return false;
    auto *r = m_context->ribbon();
    return r ? r->removeRibbonIcon(localId) : false;
}

bool Plugin::registerEmbed(const QString &ext, Markoff::EmbedFactory factory)
{
    if (!m_context) return false;
    auto *r = m_context->embeds();
    return r ? r->registerExtension(ext, std::move(factory)) : false;
}

void Plugin::unregisterEmbed(const QString &ext)
{
    if (!m_context) return;
    if (auto *r = m_context->embeds()) r->unregisterExtension(ext);
}

bool Plugin::registerMarkdownCodeBlockProcessor(
    const QString &lang, Markoff::CodeBlockProcessor proc)
{
    if (!m_context) return false;
    auto *r = m_context->codeBlocks();
    return r ? r->registerLanguage(lang, std::move(proc)) : false;
}

void Plugin::unregisterMarkdownCodeBlockProcessor(const QString &lang)
{
    if (!m_context) return;
    if (auto *r = m_context->codeBlocks()) r->unregisterLanguage(lang);
}

} // namespace Corbomite
