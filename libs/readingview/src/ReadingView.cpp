// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/readingview/ReadingView.h"

#include "SpanRenderer.h"
#include "corbomite/readingview/ReadingPipeline.h"
#include "corbomite/readingview/ReadingSection.h"
#include "corbomite/readingview/SectionLayout.h"
#include "corbomite/readingview/SectionRecyclePool.h"
#include "corbomite/readingview/VaultResourceProvider.h"
#include "corbomite/readingview/styling/StyleManager.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMouseEvent>
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
{
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

    // Diff-driven mount. Parse the new markdown, then walk each new section
    // and decide: (a) frontmatter-consumer with changed frontmatter → full
    // re-layout, (b) shape-match against an old section → reuse the
    // existing QGraphicsItem subtree (pointer-identical), (c) shape-match
    // against a pooled item → retrieve from pool, (d) otherwise → lay out
    // fresh. Unmatched old sections are offered to the pool for potential
    // reuse on a subsequent re-parse.
    auto newSections = m_pipeline->splitIntoSections(m_markdown);

    const bool fmChanged = ReadingPipeline::detectFrontmatterChange(
        m_lastMarkdown, m_markdown);

    SectionLayout::Context ctx;
    ctx.styles = m_styles.get();
    ctx.theme = m_theme;
    ctx.contentWidth = m_contentWidth;
    ctx.vaultProvider = m_vaultProvider;

    // Build an index of old sections by their renderedShape. Multiple
    // old sections may share a shape (identical content repeated); we
    // use a multi-map via QMultiHash — take removes one at a time.
    QMultiHash<QByteArray, std::shared_ptr<ReadingSection>> oldByShape;
    for (auto &old : m_sections) {
        const QByteArray key = old->renderedShape();
        if (!key.isEmpty() && old->graphicsItem() != nullptr)
            oldByShape.insert(key, old);
    }

    // Detach every known old section item from the scene BEFORE clearing
    // the scene, so we don't double-delete items we plan to reuse. We keep
    // ownership via `oldByShape` (for shape-lookup reuse) and via
    // `m_sections` (the shared_ptrs still hold graphicsItem() pointers
    // until we explicitly null them out on transfer).
    for (auto &old : m_sections) {
        if (auto *item = old->graphicsItem()) {
            if (item->scene() == s)
                s->removeItem(item);
        }
    }
    // Anything still on the scene is orphan/stray — clear deletes it.
    s->clear();

    QVector<std::shared_ptr<ReadingSection>> mounted;
    mounted.reserve(newSections.size());

    qreal y = 0.0;
    for (auto &sec : newSections) {
        const auto range = sec->sourceRange();
        const QString md =
            m_markdown.mid(range.from, range.to - range.from);

        const QByteArray shape = sec->renderedShape();
        const bool forceReRender =
            fmChanged && sec->usesFrontMatter();

        QGraphicsItem *item = nullptr;

        if (!forceReRender && !shape.isEmpty()) {
            // Try to reuse from the prior-frame section list.
            auto it = oldByShape.find(shape);
            if (it != oldByShape.end()) {
                std::shared_ptr<ReadingSection> match = it.value();
                oldByShape.erase(it);
                item = match->graphicsItem();
                match->setGraphicsItem(nullptr);
            } else if (auto *pooled = m_recyclePool->take(shape)) {
                // Try the cross-reparse recycle pool.
                item = pooled;
            }
        }

        if (!item) {
            auto *group = m_layout->layoutSection(*sec, md, ctx);
            if (!group) continue;
            item = group;
        } else {
            // Re-attach the section <-> item link so sections() exposes
            // the pointer the tests are going to assert on.
            sec->setGraphicsItem(item);
        }

        item->setPos(0, y);
        if (!item->scene())
            s->addItem(item);
        sec->setGraphicsItem(item);
        const QRectF bb = item->boundingRect();
        y += bb.height() + 4.0;
        mounted.push_back(sec);
    }

    // Old sections that didn't get reused: offer to pool for next reparse.
    for (auto it = oldByShape.begin(); it != oldByShape.end(); ++it) {
        auto &old = it.value();
        QGraphicsItem *item = old->graphicsItem();
        if (!item) continue;
        old->setGraphicsItem(nullptr);
        m_recyclePool->offer(it.key(), item);
    }

    m_sections = std::move(mounted);
    m_lastMarkdown = m_markdown;

    s->setSceneRect(0, 0, m_contentWidth, qMax<qreal>(y, 1.0));
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
