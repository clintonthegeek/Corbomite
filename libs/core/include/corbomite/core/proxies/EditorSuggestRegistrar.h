// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QVector>

namespace Corbomite {

class EditorSuggestManager;
class EditorSuggest;

/// Editor-suggest registration facade for plugins with the "ui.editor"
/// permission. EditorSuggestManager dispatches by insertion order, so
/// the manager identifies suggesters by pointer. The registrar tracks
/// the pointers it added and unregisters them all on destruction. The
/// suggester instances themselves are owned by the caller (the plugin).
class EditorSuggestRegistrar
{
public:
    EditorSuggestRegistrar(EditorSuggestManager *manager);
    ~EditorSuggestRegistrar();

    EditorSuggestRegistrar(const EditorSuggestRegistrar &) = delete;
    EditorSuggestRegistrar &operator=(const EditorSuggestRegistrar &) = delete;

    void registerSuggest(EditorSuggest *suggester);
    void unregisterSuggest(EditorSuggest *suggester);

private:
    EditorSuggestManager *m_manager;
    QVector<EditorSuggest *> m_registered;
};

} // namespace Corbomite
