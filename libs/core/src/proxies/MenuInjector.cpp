// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/MenuInjector.h"

#include "corbomite/core/MenuEventEmitter.h"

namespace Corbomite {

namespace {
QString objectId(QObject *obj)
{
    return QStringLiteral("0x%1").arg(
        reinterpret_cast<quintptr>(obj), 0, 16);
}
} // namespace

MenuInjector::MenuInjector(MenuEventEmitter *emitter) : m_emitter(emitter) {}

MenuInjector::~MenuInjector()
{
    for (const auto &c : m_connections) QObject::disconnect(c);
}

void MenuInjector::onFileMenuBuilt(FileMenuHandler handler)
{
    if (!m_emitter) return;
    auto conn = QObject::connect(m_emitter, &MenuEventEmitter::fileMenu,
        m_emitter, [h = std::move(handler)](QMenu *menu, const QString &path,
                                              const QString &source,
                                              QObject * /*leaf*/) {
            h(menu, path, source);
        });
    m_connections.append(conn);
}

void MenuInjector::onEditorMenuBuilt(Handler handler)
{
    if (!m_emitter) return;
    auto conn = QObject::connect(m_emitter, &MenuEventEmitter::editorMenu,
        m_emitter, [h = std::move(handler)](QMenu *menu, QObject *ed) {
            h(menu, objectId(ed));
        });
    m_connections.append(conn);
}

void MenuInjector::onTabMenuBuilt(Handler handler)
{
    if (!m_emitter) return;
    auto conn = QObject::connect(m_emitter, &MenuEventEmitter::leafMenu,
        m_emitter, [h = std::move(handler)](QMenu *menu, QObject *leaf) {
            h(menu, objectId(leaf));
        });
    m_connections.append(conn);
}

} // namespace Corbomite
