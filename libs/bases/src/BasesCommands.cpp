// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesCommands.h"

#include "corbomite/vault/FileManager.h"

#include <KLocalizedString>

namespace Corbomite::Bases {

CmdSetFrontMatter::CmdSetFrontMatter(Corbomite::FileManager *fm,
                                     Corbomite::TFile *file,
                                     QString key,
                                     QVariant newValue,
                                     std::function<void(const QString &)> notify)
    : m_fm(fm),
      m_file(file),
      m_key(std::move(key)),
      m_newValue(std::move(newValue)),
      m_notify(std::move(notify))
{
    setText(i18n("Edit \"%1\"", m_key));
}

void CmdSetFrontMatter::redo()
{
    if (m_neutralized) return;
    bool stale = false;
    const bool ok = m_fm->processFrontMatter(m_file, [&](QVariantMap &fm) {
        const QVariant current = fm.value(m_key);
        if (!m_oldCaptured) {
            // First application (the QUndoStack::push): capture pre-state and
            // apply unconditionally.
            m_oldValue = current;
            m_oldCaptured = true;
        } else if (current != m_oldValue) {
            // Re-redo after an undo, but the field drifted since undo left it
            // at oldValue.
            stale = true;
            return;
        }
        fm.insert(m_key, m_newValue);
    });
    if (!ok || stale) {
        m_neutralized = true;
        if (m_notify)
            m_notify(i18n("Skipped redo — \"%1\" changed outside Bases", m_key));
    }
}

void CmdSetFrontMatter::undo()
{
    if (m_neutralized) return;
    bool stale = false;
    const bool ok = m_fm->processFrontMatter(m_file, [&](QVariantMap &fm) {
        const QVariant current = fm.value(m_key);
        if (current != m_newValue) {
            // Someone changed the field since redo wrote newValue.
            stale = true;
            return;
        }
        if (m_oldValue.isValid())
            fm.insert(m_key, m_oldValue);
        else
            fm.remove(m_key);   // key did not exist before the edit
    });
    if (!ok || stale) {
        m_neutralized = true;
        if (m_notify)
            m_notify(i18n("Skipped undo — \"%1\" changed outside Bases", m_key));
    }
}

}  // namespace Corbomite::Bases
