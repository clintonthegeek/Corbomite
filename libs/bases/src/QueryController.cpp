// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/QueryController.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/FunctionRegistry.h"

#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <QTimer>

namespace Corbomite::Bases {

QueryController::QueryController(Vault *vault, MetadataCache *cache,
                                 FunctionRegistry *funcs, QObject *parent)
    : QObject(parent),
      m_vault(vault),
      m_cache(cache),
      m_funcs(funcs ? funcs : &FunctionRegistry::global()),
      m_recomputeTimer(new QTimer(this))
{
    m_recomputeTimer->setSingleShot(true);
    m_recomputeTimer->setInterval(50);  // 50ms debounce
    connect(m_recomputeTimer, &QTimer::timeout, this,
            &QueryController::onDebouncedRecompute);
    if (m_cache) {
        connect(m_cache, &MetadataCache::cacheChanged, this,
                &QueryController::onCacheChanged);
        connect(m_cache, &MetadataCache::cacheDeleted, this,
                &QueryController::onCacheDeleted);
    }
}

QueryController::~QueryController() = default;

void QueryController::setQuery(std::shared_ptr<BasesQuery> query)
{
    m_query = std::move(query);
    if (m_query && !m_query->views.empty() && !m_cfg)
        m_cfg = m_query->views.front().get();
    scheduleRecompute();
}

void QueryController::setViewConfig(BasesViewConfig *cfg)
{
    m_cfg = cfg;
    scheduleRecompute();
}

void QueryController::setCurrentFile(TFile *local)
{
    m_local = local;
    scheduleRecompute();
}

void QueryController::setSearchQuery(const QString &q)
{
    if (m_searchQuery == q) return;
    m_searchQuery = q;
    scheduleRecompute();
}

void QueryController::onCacheChanged() { scheduleRecompute(); }
void QueryController::onCacheDeleted() { scheduleRecompute(); }

void QueryController::scheduleRecompute()
{
    if (!m_recomputeTimer->isActive()) m_recomputeTimer->start();
}

void QueryController::onDebouncedRecompute() { recomputeNow(); }

void QueryController::recomputeNow()
{
    if (!m_query || !m_cfg || !m_vault) {
        m_result.reset();
        Q_EMIT resultsChanged();
        return;
    }

    // Gather candidate TFiles — markdown only per audit surface.
    const QVector<TFile *> files = m_vault->getMarkdownFiles();

    // One resolver per recompute, seeded from the full vault path set.
    m_resolver = std::make_unique<BasesVaultResolver>(m_vault, m_cache);

    QVector<std::shared_ptr<BasesEntry>> entries;
    entries.reserve(files.size());
    for (TFile *f : files) {
        entries.push_back(std::make_shared<BasesEntry>(
            m_vault, m_cache, f, m_local ? m_local : f, *m_query, m_funcs,
            m_resolver.get()));
    }

    // Apply global filter + per-view filter (both AND).
    QVector<std::shared_ptr<BasesEntry>> filtered;
    filtered.reserve(entries.size());
    for (const auto &e : entries) {
        if (m_query->filters && !m_query->filters->test(*e, m_funcs)) continue;
        if (m_cfg->filters && !m_cfg->filters->test(*e, m_funcs)) continue;
        // Search over visible cells (simple contains across displayed
        // properties' toString()). Skip when query is empty.
        if (!m_searchQuery.isEmpty()) {
            bool hit = false;
            for (const auto &p : m_cfg->order) {
                const auto v = e->getValue(p);
                if (v && v->toString().contains(m_searchQuery, Qt::CaseInsensitive)) {
                    hit = true;
                    break;
                }
            }
            if (!hit) continue;
        }
        filtered.push_back(e);
    }

    m_result = std::make_unique<BasesQueryResult>(
        *m_cfg, filtered, m_funcs,
        m_query ? &m_query->summaryFormulas : nullptr);
    Q_EMIT resultsChanged();
}

}  // namespace Corbomite::Bases
