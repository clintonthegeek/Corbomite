// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/EditorSuggestRegistrar.h"

#include "corbomite/core/EditorSuggestManager.h"

namespace Corbomite {

EditorSuggestRegistrar::EditorSuggestRegistrar(EditorSuggestManager *manager)
    : m_manager(manager)
{}

EditorSuggestRegistrar::~EditorSuggestRegistrar()
{
    if (!m_manager) return;
    for (int i = m_registered.size() - 1; i >= 0; --i) {
        m_manager->unregisterSuggest(m_registered.at(i));
    }
}

void EditorSuggestRegistrar::registerSuggest(EditorSuggest *suggester)
{
    if (!m_manager || !suggester) return;
    m_manager->registerSuggest(suggester);
    m_registered.append(suggester);
}

void EditorSuggestRegistrar::unregisterSuggest(EditorSuggest *suggester)
{
    if (!m_manager || !suggester) return;
    m_manager->unregisterSuggest(suggester);
    m_registered.removeAll(suggester);
}

} // namespace Corbomite
