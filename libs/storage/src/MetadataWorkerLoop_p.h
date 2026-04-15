// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Private implementation header for MetadataWorker. Not installed; not part of
// the public API. Exists so AUTOMOC can find the Q_OBJECT on the worker-loop
// class that lives on the worker thread.

#include "corbomite/storage/CachedMetadata.h"

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtCore/QString>
#include <QtCore/QWaitCondition>

#include <atomic>

namespace Corbomite {

class LinkResolver;

struct MetadataWorkerItem {
    QString path;
    QByteArray content;
    qint64 mtimeMs = 0;
    qint64 size = 0;
};

/// The object that actually lives on the worker thread. Receives enqueue
/// notifications via the wait condition; pops work items one at a time; runs
/// MetadataParser::parse; emits parsed() which queued-connects to the main
/// thread.
class MetadataWorkerLoop : public QObject {
    Q_OBJECT
public:
    MetadataWorkerLoop(const LinkResolver &resolver,
                       QMutex *mutex,
                       QWaitCondition *cond,
                       QQueue<MetadataWorkerItem> *queue,
                       std::atomic<bool> *stopping);

Q_SIGNALS:
    void parsed(const QString &path,
                qint64 mtimeMs,
                qint64 size,
                const Corbomite::CachedMetadata &cache,
                const QString &hash);

public Q_SLOTS:
    /// Entered once on worker-thread startup. Loops until *m_stopping is true.
    /// Items still in the queue when stopping fires are discarded.
    void run();

private:
    const LinkResolver &m_resolver;
    QMutex *m_mutex;
    QWaitCondition *m_cond;
    QQueue<MetadataWorkerItem> *m_queue;
    std::atomic<bool> *m_stopping;
};

}  // namespace Corbomite
