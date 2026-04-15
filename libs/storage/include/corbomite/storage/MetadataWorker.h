// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/storage/CachedMetadata.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace Corbomite {

class LinkResolver;
class MetadataWorkerPrivate;

/// Background thread that serialises MetadataParser::parse calls. Owns its own
/// QThread. Main-thread API: enqueueParse() to append, parsed() signal to
/// receive results via Qt::QueuedConnection. The worker processes one request
/// at a time -- concurrent enqueues serialise through a FIFO queue, matching
/// Obsidian's "Work queue must be sequential!" invariant.
///
/// Lifetime: construct on main thread; destructor blocks until in-flight parse
/// completes and the worker thread joins. Work items still sitting in the
/// queue when the destructor runs are discarded (not parsed, no `parsed()`
/// emission).
///
/// Thread-safety of the LinkResolver: `resolver` must outlive this worker.
/// The worker calls `resolver.resolve` on the worker thread; the caller is
/// responsible for ensuring resolver mutation (addVaultPath / removeVaultPath
/// / setVaultPaths) happens only while the queue is idle, or is itself
/// externally synchronised.
class MetadataWorker : public QObject {
    Q_OBJECT
public:
    explicit MetadataWorker(const LinkResolver &resolver, QObject *parent = nullptr);
    ~MetadataWorker() override;

    MetadataWorker(const MetadataWorker &) = delete;
    MetadataWorker &operator=(const MetadataWorker &) = delete;

    /// Main-thread API. Appends to the worker queue. Returns immediately.
    /// `size` is threaded through the signal so consumers do not have to
    /// stash per-path state while the parse is in flight.
    void enqueueParse(const QString &path,
                      const QByteArray &content,
                      qint64 mtimeMs,
                      qint64 size);

    /// Introspection -- current pending count. Main-thread safe.
    int pendingCount() const;

Q_SIGNALS:
    /// Emitted via Qt::QueuedConnection from the worker thread to the main
    /// thread after each parse completes. `cache` is the parsed metadata;
    /// `hash` is the 64-char lowercase SHA-256 hex of the content bytes.
    /// `size` is the byte count of the content that was parsed (threaded
    /// through from enqueueParse so downstream consumers can stash stat
    /// bookkeeping without per-path side state).
    void parsed(const QString &path,
                qint64 mtimeMs,
                qint64 size,
                const Corbomite::CachedMetadata &cache,
                const QString &hash);

private:
    MetadataWorkerPrivate *d;
};

}  // namespace Corbomite
