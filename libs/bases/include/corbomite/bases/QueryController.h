// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQuery.h"
#include "BasesQueryResult.h"

#include <QObject>
#include <QString>

#include <memory>

class QTimer;

namespace Corbomite {
class MetadataCache;
class TFile;
class Vault;
}  // namespace Corbomite

namespace Corbomite::Bases {

class FunctionRegistry;

/// Incremental-refresh driver for a single BasesView.
///
/// Subscribes to MetadataCache cacheChanged/cacheDeleted signals and
/// schedules a debounced recompute. Emits `resultsChanged()` when the
/// computed BasesQueryResult has refreshed.
class QueryController : public QObject
{
    Q_OBJECT
public:
    QueryController(Vault *vault,
                    MetadataCache *cache,
                    FunctionRegistry *funcs = nullptr,
                    QObject *parent = nullptr);
    ~QueryController() override;

    void setQuery(std::shared_ptr<BasesQuery> query);
    void setViewConfig(BasesViewConfig *cfg);
    void setCurrentFile(TFile *local);
    void setSearchQuery(const QString &q);

    const BasesQuery *query() const { return m_query.get(); }
    const BasesViewConfig *viewConfig() const { return m_cfg; }
    const BasesQueryResult *result() const { return m_result.get(); }

    /// Force immediate recompute (tests + explicit refresh).
    void recomputeNow();

Q_SIGNALS:
    void resultsChanged();

private Q_SLOTS:
    void onCacheChanged();
    void onCacheDeleted();
    void onDebouncedRecompute();

private:
    void scheduleRecompute();

    Vault *m_vault;
    MetadataCache *m_cache;
    FunctionRegistry *m_funcs;

    std::shared_ptr<BasesQuery> m_query;
    BasesViewConfig *m_cfg = nullptr;
    TFile *m_local = nullptr;
    QString m_searchQuery;

    std::unique_ptr<BasesQueryResult> m_result;
    QTimer *m_recomputeTimer = nullptr;
};

}  // namespace Corbomite::Bases
