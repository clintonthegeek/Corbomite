// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/readingview/ReadingView.h"

#include "corbomite/readingview/ReadingPipeline.h"
#include "corbomite/readingview/ReadingSection.h"
#include "corbomite/readingview/SectionLayout.h"
#include "corbomite/readingview/styling/StyleManager.h"

#include <QFontMetricsF>
#include <QGraphicsItemGroup>
#include <QGraphicsScene>
#include <QResizeEvent>
#include <QScrollBar>

namespace Corbomite::ReadingView {

ReadingView::ReadingView(QWidget *parent)
    : QGraphicsView(parent)
    , m_pipeline(std::make_unique<ReadingPipeline>(this))
    , m_layout(std::make_unique<SectionLayout>())
    , m_styles(std::unique_ptr<StyleManager>(
          StyleManager::makeObsidianDefault(Theme::Light)))
{
    auto *scene = new QGraphicsScene(this);
    setScene(scene);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

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
    rebuild();
}

Theme ReadingView::theme() const { return m_theme; }

void ReadingView::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;
    m_styles.reset(StyleManager::makeObsidianDefault(theme));
    rebuild();
}

void ReadingView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    // Phase 3a: we don't auto-adjust contentWidth to viewport width. Caller
    // drives contentWidth via setContentWidth() if they want reflow. Phase
    // 6's VirtualScrollController is the right place to hook width changes.
}

void ReadingView::rebuild()
{
    auto *s = scene();
    if (!s) return;

    s->clear();
    m_sections.clear();
    // SectionLayout owns highlighters; reset it to dispose of previous-run's
    // highlighter objects before the new render.
    m_layout = std::make_unique<SectionLayout>();

    auto sections = m_pipeline->splitIntoSections(m_markdown);

    SectionLayout::Context ctx;
    ctx.styles = m_styles.get();
    ctx.theme = m_theme;
    ctx.contentWidth = m_contentWidth;

    qreal y = 0.0;
    for (auto &sec : sections) {
        const auto range = sec->sourceRange();
        const QString md =
            m_markdown.mid(range.from, range.to - range.from);
        auto *group = m_layout->layoutSection(*sec, md, ctx);
        if (!group) continue;
        group->setPos(0, y);
        s->addItem(group);
        sec->setGraphicsItem(group);
        const QRectF bb = group->boundingRect();
        y += bb.height() + 4.0;
        m_sections.push_back(sec);
    }

    s->setSceneRect(0, 0, m_contentWidth, qMax<qreal>(y, 1.0));
}

qreal ReadingView::visualLineSpacing() const
{
    // Use the Body style's font to approximate visual-line height.
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

} // namespace Corbomite::ReadingView
