// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/RecoveryService.h"

#include "corbomite/core/TextFileView.h"

#include <QTimer>

namespace Corbomite {

RecoveryService::RecoveryService(ViewProvider provider, QObject *parent)
    : QObject(parent)
    , m_provider(std::move(provider))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &RecoveryService::snapshotNow);
}

void RecoveryService::setIntervalMinutes(int minutes)
{
    m_intervalMinutes = minutes < 0 ? 0 : minutes;
    if (m_intervalMinutes == 0) {
        m_timer->stop();
        return;
    }
    m_timer->setInterval(m_intervalMinutes * 60 * 1000);
    if (m_timer->isActive())
        m_timer->start();  // re-arm with the new interval
}

void RecoveryService::start()
{
    if (m_intervalMinutes <= 0)
        return;
    m_timer->start(m_intervalMinutes * 60 * 1000);
}

void RecoveryService::stop()
{
    m_timer->stop();
}

bool RecoveryService::isRunning() const
{
    return m_timer->isActive();
}

int RecoveryService::snapshotNow()
{
    if (!m_provider)
        return 0;

    int written = 0;
    for (TextFileView *view : m_provider()) {
        if (!view || !view->isDirty())
            continue;
        const QString data = view->getViewData();
        if (m_lastSnapshot.value(view) == data)
            continue;  // unchanged since the last snapshot this run
        view->writeRecoverySnapshot(data, /*failure=*/false);
        m_lastSnapshot.insert(view, data);
        ++written;
    }
    return written;
}

}  // namespace Corbomite
