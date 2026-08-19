// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasMermaidAdapter.h"

#include "corbomite/core/MermaidRenderer.h"

#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

namespace Corbomite {

CanvasMermaidAdapter::CanvasMermaidAdapter(const Core::MermaidRenderer *renderer)
    : m_renderer(renderer)
{
}

QPixmap CanvasMermaidAdapter::render(const QString &source) const
{
    if (!m_renderer) return {};

    const QByteArray svgData = m_renderer->renderSvg(source);
    if (svgData.isEmpty()) return {};

    QSvgRenderer svgRenderer(svgData);
    if (!svgRenderer.isValid()) return {};

    QSize size = svgRenderer.defaultSize();
    if (size.isEmpty()) size = QSize(600, 400);

    // Scale down oversized diagrams, same budget as the Reading-mode
    // mermaid path (MarkdownRenderer.cpp's renderMermaidToDataUri).
    if (size.width() > 800) {
        const double scale = 800.0 / size.width();
        size = QSize(800, static_cast<int>(size.height() * scale));
    }

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    svgRenderer.render(&painter);
    painter.end();

    if (img.isNull()) return {};
    return QPixmap::fromImage(img);
}

} // namespace Corbomite
