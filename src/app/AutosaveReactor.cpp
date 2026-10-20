// SPDX-License-Identifier: GPL-3.0-or-later
#include "AutosaveReactor.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/vault/Vault.h"

namespace Corbomite {

AutosaveReactor::AutosaveReactor(Vault *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
}

void AutosaveReactor::watchDocument(NoteDocument *doc)
{
    if (m_timers.contains(doc)) return;

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(m_delayMs);
    m_timers.insert(doc, timer);

    connect(timer, &QTimer::timeout, this, [this, doc]() {
        if (doc->isModified()) {
            m_vault->saveDocument(doc);
            Q_EMIT noteSaved(doc->relativePath());
        }
    });

    connect(doc, &NoteDocument::textChanged, this, [this, doc]() {
        if (auto *t = m_timers.value(doc)) {
            t->start(); // Restart debounce timer
        }
    });
}

void AutosaveReactor::unwatchDocument(NoteDocument *doc)
{
    if (auto *timer = m_timers.take(doc)) {
        timer->stop();
        timer->deleteLater();
    }
}

void AutosaveReactor::setDelayMs(int ms)
{
    m_delayMs = ms;
    for (auto *timer : m_timers) {
        timer->setInterval(ms);
    }
}

} // namespace Corbomite
