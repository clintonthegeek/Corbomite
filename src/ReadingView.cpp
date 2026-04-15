// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/readingview/ReadingView.h"

#include "SpanRenderer.h"
#include "corbomite/readingview/ReadingParseWorker.h"
#include "corbomite/readingview/ReadingPipeline.h"
#include "corbomite/readingview/ReadingSection.h"
#include "corbomite/readingview/ReadingViewConstants.h"
#include "corbomite/readingview/SectionLayout.h"
#include "corbomite/readingview/SectionRecyclePool.h"
#include "corbomite/readingview/VaultResourceProvider.h"
#include "corbomite/readingview/styling/StyleManager.h"

#include <QAbstractTextDocumentLayout>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

namespace Corbomite::ReadingView {

ReadingView::ReadingView(QWidget *parent)
    : QGraphicsView(parent)
    , m_pipeline(std::make_unique<ReadingPipeline>(this))
    , m_layout(std::make_unique<SectionLayout>())
    , m_styles(std::unique_ptr<StyleManager>(
          StyleManager::makeObsidianDefault(Theme::Light)))
    , m_recyclePool(std::make_unique<SectionRecyclePool>())
    , m_worker(std::make_unique<ReadingParseWorker>())
{
    // Worker's parseFinished emits from its own thread; Qt::QueuedConnection
    // hops to ours so the UI mount loop stays on the main thread.
    connect(m_worker.get(), &ReadingParseWorker::parseFinished,
            this, &ReadingView::onParseFinished,
            Qt::QueuedConnection);
    auto *scene = new QGraphicsScene(this);
    setScene(scene);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setMouseTracking(true);

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(300);
    connect(m_hoverTimer, &QTimer::timeout, this, [this] {
        if (!m_pendingHoverTarget.isEmpty())
            Q_EMIT wikiLinkHovered(m_pendingHoverTarget);
    });

    if (auto *vbar = verticalScrollBar()) {
        connect(vbar, &QScrollBar::valueChanged, this, [this] {
            Q_EMIT scrollPositionVisualLineChanged(scrollPositionVisualLine());
        });
    }
}

ReadingView::~ReadingView() = default;

void ReadingView::setPlainText(const QString &markdown)
{
    m_markdown = markdown;
    rebuild();
}

qreal ReadingView::contentWidth() const { return m_contentWidth; }

void ReadingView::setContentWidth(qreal width)
{
    if (qFuzzyCompare(width, m_contentWidth)) return;
    m_contentWidth = width;
    // Layout context changed; rendered-shape cache would compare bogusly.
    m_recyclePool->clear();
    m_lastMarkdown.clear();
    m_layout = std::make_unique<SectionLayout>();
    rebuild();
}

Theme ReadingView::theme() const { return m_theme; }

void ReadingView::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;
    m_styles.reset(StyleManager::makeObsidianDefault(theme));
    // Styles changed — every mounted item is wrong now.
    m_recyclePool->clear();
    m_lastMarkdown.clear();
    m_layout = std::make_unique<SectionLayout>();
    rebuild();
}

void ReadingView::setVaultResourceProvider(VaultResourceProvider *provider)
{
    m_vaultProvider = provider;
    m_recyclePool->clear();
    m_lastMarkdown.clear();
    m_layout = std::make_unique<SectionLayout>();
    rebuild();
}

int ReadingView::recyclePoolSize() const
{
    return m_recyclePool ? m_recyclePool->size() : 0;
}

VaultResourceProvider *ReadingView::vaultResourceProvider() const
{
    return m_vaultProvider;
}

void ReadingView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
}

void ReadingView::rebuild()
{
    auto *s = scene();
    if (!s) return;

    // Phase 5: parse gate. Notes at or above `kAsyncParseThresholdBytes`
    // (10240) go to the worker; smaller notes parse sync on this thread.
    // Either way the mount loop is frame-budgeted.
    //
    // `toUtf8().size()` is the byte length Obsidian's threshold is specified
    // in. QString::size() would count UTF-16 code units — slightly different
    // for notes with multi-byte characters, enough to push an edge-case note
    // over the threshold in one direction on the wire and the other in our
    // check. Matching the contract means matching on UTF-8 bytes.
    const int byteLen = m_markdown.toUtf8().size();
    const quint64 requestId = ++m_requestIdCounter;

    if (byteLen >= kAsyncParseThresholdBytes) {
        // Align the worker's internal latest-id with ours so its coalescing
        // matches the UI-level coalescing (belt-and-suspenders).
        while (m_worker->bumpRequestId() < requestId) { /* catch up */ }
        m_worker->parseAsync(m_markdown, requestId);
        // Mount happens when parseFinished fires. Return now so the UI
        // thread stays free.
        return;
    }

    // Sync path — < 10240 bytes.
    auto newSections = m_worker->parseSync(m_markdown);
    m_lastRequestIdHandled = requestId;
    beginMount(std::move(newSections));
}

void ReadingView::onParseFinished(
    quint64 requestId,
    QVector<std::shared_ptr<ReadingSection>> sections)
{
    // UI-level coalescing: ignore anything older than what we've already
    // seen, and anything older than the latest requestId counter (the
    // worker does its own check, but queued signals already in flight
    // when a newer parseAsync fires can still arrive here).
    if (requestId <= m_lastRequestIdHandled) return;
    if (requestId < m_requestIdCounter) return;
    m_lastRequestIdHandled = requestId;
    beginMount(std::move(sections));
}

void ReadingView::beginMount(
    QVector<std::shared_ptr<ReadingSection>> newSections)
{
    auto *s = scene();
    if (!s) return;

    m_pendingFmChanged = ReadingPipeline::detectFrontmatterChange(
        m_lastMarkdown, m_markdown);

    // Build an index of old sections by their renderedShape. Multiple old
    // sections may share a shape; QMultiHash resolves one at a time via
    // `take`.
    m_oldByShape.clear();
    for (auto &old : m_sections) {
        const QByteArray key = old->renderedShape();
        if (!key.isEmpty() && old->graphicsItem() != nullptr)
            m_oldByShape.insert(key, old);
    }

    // Detach every known old section item from the scene BEFORE clearing
    // the scene, so we don't double-delete items we plan to reuse.
    for (auto &old : m_sections) {
        if (auto *item = old->graphicsItem()) {
            if (item->scene() == s)
                s->removeItem(item);
        }
    }
    // Anything still on the scene is orphan/stray — clear deletes it.
    s->clear();

    m_pendingSections = std::move(newSections);
    m_mountedSoFar.clear();
    m_mountedSoFar.reserve(m_pendingSections.size());
    m_mountY = 0.0;
    m_mountInProgress = true;

    // `m_sections` is only updated when the mount completes — callers that
    // observe `sections()` during a partial mount see the previous list
    // (typically empty on first load). This avoids tests and callers
    // seeing half-mounted entries with `graphicsItem() == nullptr`.
    m_sections.clear();
    m_lastMarkdown = m_markdown;

    // Seed the scene rect so scrolling-during-partial-mount doesn't snap.
    s->setSceneRect(0, 0, m_contentWidth, 1.0);

    mountSectionsWithBudget(0);
}

void ReadingView::mountSectionsWithBudget(int startIdx)
{
    auto *s = scene();
    if (!s) { m_mountInProgress = false; return; }

    SectionLayout::Context ctx;
    ctx.styles = m_styles.get();
    ctx.theme = m_theme;
    ctx.contentWidth = m_contentWidth;
    ctx.vaultProvider = m_vaultProvider;

    QElapsedTimer t;
    t.start();
    int sectionsThisFrame = 0;
    int layoutsThisFrame = 0; // layouts count against the 5 ms budget

    for (int i = startIdx; i < m_pendingSections.size(); ++i) {
        auto &sec = m_pendingSections[i];
        const auto range = sec->sourceRange();
        const QString md =
            m_markdown.mid(range.from, range.to - range.from);

        const QByteArray shape = sec->renderedShape();
        const bool forceReRender =
            m_pendingFmChanged && sec->usesFrontMatter();

        QGraphicsItem *item = nullptr;
        bool wasRecycled = false;

        if (!forceReRender && !shape.isEmpty()) {
            auto it = m_oldByShape.find(shape);
            if (it != m_oldByShape.end()) {
                std::shared_ptr<ReadingSection> match = it.value();
                m_oldByShape.erase(it);
                item = match->graphicsItem();
                match->setGraphicsItem(nullptr);
                wasRecycled = true;
            } else if (auto *pooled = m_recyclePool->take(shape)) {
                item = pooled;
                wasRecycled = true;
            }
        }

        if (!item) {
            auto *group = m_layout->layoutSection(*sec, md, ctx);
            if (!group) {
                // Skip — do not count toward budget. Should be rare.
                continue;
            }
            item = group;
            ++layoutsThisFrame;
        } else {
            sec->setGraphicsItem(item);
        }

        item->setPos(0, m_mountY);
        if (!item->scene())
            s->addItem(item);
        sec->setGraphicsItem(item);
        const QRectF bb = item->boundingRect();
        m_mountY += bb.height() + 4.0;
        m_mountedSoFar.push_back(sec);
        ++sectionsThisFrame;
        Q_UNUSED(wasRecycled);

        // Grow the scene rect as we mount so scroll-during-partial-mount
        // reflects the actual geometry.
        s->setSceneRect(0, 0, m_contentWidth, qMax<qreal>(m_mountY, 1.0));

        // Yield-check: stop after this iteration if either limit hit.
        // Recycled sections are microseconds each, so we only enforce the
        // 5 ms budget when an actual SectionLayout::layoutSection call ran.
        const bool timerBudgetSpent =
            layoutsThisFrame > 0 && t.elapsed() >= kFrameBudgetMs;
        const bool sectionBudgetSpent =
            sectionsThisFrame >= kFrameBudgetSections;

        if ((timerBudgetSpent || sectionBudgetSpent)
            && i + 1 < m_pendingSections.size()) {
            const int resumeIdx = i + 1;
            // Publish the partial mounted list so observers calling
            // `sections()` during the yield see only fully-mounted entries
            // (no half-built section with `graphicsItem() == nullptr`).
            m_sections = m_mountedSoFar;
            QPointer<ReadingView> guard(this);
            QTimer::singleShot(0, this, [guard, resumeIdx] {
                if (guard) guard->mountSectionsWithBudget(resumeIdx);
            });
            return;
        }
    }

    // All sections mounted. Finalise: drain leftover old-by-shape entries
    // into the recycle pool for next reparse, commit the mounted list,
    // and emit the completion signal.
    for (auto it = m_oldByShape.begin(); it != m_oldByShape.end(); ++it) {
        auto &old = it.value();
        QGraphicsItem *item = old->graphicsItem();
        if (!item) continue;
        old->setGraphicsItem(nullptr);
        m_recyclePool->offer(it.key(), item);
    }
    m_oldByShape.clear();

    m_sections = std::move(m_mountedSoFar);
    m_pendingSections.clear();
    m_mountInProgress = false;

    s->setSceneRect(0, 0, m_contentWidth, qMax<qreal>(m_mountY, 1.0));

    Q_EMIT mountingFinished();
}

qreal ReadingView::visualLineSpacing() const
{
    const ParagraphStyle body =
        const_cast<StyleManager *>(m_styles.get())
            ->resolvedParagraphStyle(QStringLiteral("Body"));
    QFont font;
    if (body.hasFontFamily()) font.setFamily(body.fontFamily());
    if (body.hasFontSize()) font.setPointSizeF(body.fontSize());
    else font.setPointSizeF(14);
    const QFontMetricsF fm(font);
    return qMax<qreal>(fm.lineSpacing(), 1.0);
}

float ReadingView::scrollPositionVisualLine() const
{
    auto *vbar = verticalScrollBar();
    if (!vbar) return 0.0f;
    const qreal pixels = vbar->value();
    return static_cast<float>(pixels / visualLineSpacing());
}

void ReadingView::setScrollPositionVisualLine(float visualLine)
{
    auto *vbar = verticalScrollBar();
    if (!vbar) return;
    const qreal pixels = visualLine * visualLineSpacing();
    vbar->setValue(qRound(pixels));
}

QString ReadingView::wikiLinkTargetAt(const QPoint &viewportPos) const
{
    auto *s = scene();
    if (!s) return {};
    const QPointF scenePos = mapToScene(viewportPos);
    // Walk all items at this point and look for a QGraphicsTextItem whose
    // document fragment under the hit position carries the wiki-link target
    // property.
    const QList<QGraphicsItem *> hits = s->items(scenePos);
    for (QGraphicsItem *it : hits) {
        auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(it);
        if (!ti) continue;
        const QPointF itemPos = ti->mapFromScene(scenePos);
        QTextDocument *doc = ti->document();
        if (!doc) continue;
        const int cursor = doc->documentLayout()->hitTest(
            itemPos, Qt::FuzzyHit);
        if (cursor < 0) continue;
        QTextCursor tc(doc);
        tc.setPosition(cursor);
        const QTextCharFormat cf = tc.charFormat();
        const QVariant v = cf.property(SpanRenderer::WikiLinkTargetProperty);
        if (v.isValid() && !v.toString().isEmpty())
            return v.toString();
    }
    return {};
}

void ReadingView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QString target = wikiLinkTargetAt(event->pos());
        if (!target.isEmpty()) {
            Q_EMIT wikiLinkActivated(target);
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void ReadingView::mouseMoveEvent(QMouseEvent *event)
{
    const QString target = wikiLinkTargetAt(event->pos());
    if (target != m_pendingHoverTarget) {
        m_pendingHoverTarget = target;
        if (!target.isEmpty())
            m_hoverTimer->start();
        else
            m_hoverTimer->stop();
    }
    QGraphicsView::mouseMoveEvent(event);
}

} // namespace Corbomite::ReadingView
