// SPDX-License-Identifier: GPL-3.0-or-later
//
// MetadataWorker -- dedicated QThread + mutex + condition-variable queue that
// serialises MetadataParser::parse calls off the caller's thread.
//
// Design:
//   * The worker thread runs a plain condition-variable loop (not
//     QThread::exec) so it can cleanly observe the `stopping` flag on
//     shutdown without relying on event-loop quit semantics.
//   * MetadataWorkerLoop holds raw pointers to the mutex / condition / queue
//     / stopping flag that live on MetadataWorkerPrivate. The pimpl outlives
//     the loop (deletion is via QThread::finished -> deleteLater, and the
//     destructor waits for the thread to join before destroying the pimpl).
//   * Destructor invariant: stopping := true, wakeAll, thread->quit() +
//     thread->wait(). Any in-flight parse completes; queued-but-unstarted
//     items are dropped.

#include "corbomite/storage/MetadataWorker.h"

#include "MetadataWorkerLoop_p.h"

#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataParser.h"

#include <QtCore/QMetaType>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QQueue>
#include <QtCore/QThread>
#include <QtCore/QWaitCondition>

#include <atomic>
#include <utility>

namespace Corbomite {

// ---------------------------------------------------------------------------
// MetadataWorkerLoop
// ---------------------------------------------------------------------------

MetadataWorkerLoop::MetadataWorkerLoop(const LinkResolver &resolver,
                                       QMutex *mutex,
                                       QWaitCondition *cond,
                                       QQueue<MetadataWorkerItem> *queue,
                                       std::atomic<bool> *stopping)
    : m_resolver(resolver)
    , m_mutex(mutex)
    , m_cond(cond)
    , m_queue(queue)
    , m_stopping(stopping)
{
}

void MetadataWorkerLoop::run()
{
    for (;;) {
        MetadataWorkerItem item;
        {
            QMutexLocker lock(m_mutex);
            while (m_queue->isEmpty()
                   && !m_stopping->load(std::memory_order_acquire)) {
                m_cond->wait(m_mutex);
            }
            if (m_stopping->load(std::memory_order_acquire)) {
                // Stop requested. Drop remaining queued items -- we do not
                // emit parsed() for them; the destructor contract says
                // post-stop emissions must not hit a dying MetadataCache.
                return;
            }
            item = m_queue->dequeue();
        }

        // Parse outside the lock.
        const ParsedNote result =
            MetadataParser::parse(item.content, item.path, m_resolver);

        Q_EMIT parsed(item.path,
                      item.mtimeMs,
                      item.size,
                      result.cache,
                      result.hash);
    }
}

// ---------------------------------------------------------------------------
// MetadataWorkerPrivate
// ---------------------------------------------------------------------------

class MetadataWorkerPrivate {
public:
    MetadataWorkerPrivate(MetadataWorker *q, const LinkResolver &resolver);
    ~MetadataWorkerPrivate();

    void enqueue(MetadataWorkerItem item);
    int pendingCount() const;

    MetadataWorker *q;
    mutable QMutex mutex;
    QWaitCondition cond;
    QQueue<MetadataWorkerItem> queue;
    std::atomic<bool> stopping{false};
    QThread *thread = nullptr;
    MetadataWorkerLoop *loop = nullptr;
};

MetadataWorkerPrivate::MetadataWorkerPrivate(MetadataWorker *q_,
                                             const LinkResolver &resolver)
    : q(q_)
{
    // Ensure the custom types cross thread boundaries via queued connections.
    qRegisterMetaType<Corbomite::CachedMetadata>("Corbomite::CachedMetadata");

    thread = new QThread(q);
    thread->setObjectName(QStringLiteral("Corbomite.MetadataWorker"));

    loop = new MetadataWorkerLoop(resolver, &mutex, &cond, &queue, &stopping);
    loop->moveToThread(thread);

    // Relay worker->main parsed signal via queued connection, re-emit as
    // MetadataWorker::parsed on the main thread.
    QObject::connect(loop, &MetadataWorkerLoop::parsed,
                     q, &MetadataWorker::parsed,
                     Qt::QueuedConnection);

    // Start the loop once the thread's event loop is running.
    QObject::connect(thread, &QThread::started,
                     loop, &MetadataWorkerLoop::run);
    // Delete the loop object on the worker thread once the thread stops.
    QObject::connect(thread, &QThread::finished,
                     loop, &QObject::deleteLater);

    thread->start();
}

MetadataWorkerPrivate::~MetadataWorkerPrivate()
{
    // Signal stop, wake the worker, then join.
    {
        QMutexLocker lock(&mutex);
        stopping.store(true, std::memory_order_release);
        cond.wakeAll();
    }
    // Ask the worker thread's event loop to exit after run() returns. quit()
    // is a no-op until the event loop processes it, which happens after our
    // run() loop naturally returns from seeing the stopping flag.
    thread->quit();
    thread->wait();
    // `thread` is parented to `q` and will be destroyed with it; `loop` was
    // deleted via deleteLater on QThread::finished before wait() returned.
}

void MetadataWorkerPrivate::enqueue(MetadataWorkerItem item)
{
    QMutexLocker lock(&mutex);
    queue.enqueue(std::move(item));
    cond.wakeOne();
}

int MetadataWorkerPrivate::pendingCount() const
{
    QMutexLocker lock(&mutex);
    return queue.size();
}

// ---------------------------------------------------------------------------
// MetadataWorker (public facade)
// ---------------------------------------------------------------------------

MetadataWorker::MetadataWorker(const LinkResolver &resolver, QObject *parent)
    : QObject(parent)
    , d(new MetadataWorkerPrivate(this, resolver))
{
}

MetadataWorker::~MetadataWorker()
{
    delete d;
}

void MetadataWorker::enqueueParse(const QString &path,
                                  const QByteArray &content,
                                  qint64 mtimeMs,
                                  qint64 size)
{
    d->enqueue(MetadataWorkerItem{path, content, mtimeMs, size});
}

int MetadataWorker::pendingCount() const
{
    return d->pendingCount();
}

}  // namespace Corbomite
