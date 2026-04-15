// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/readingview/CodeBlockHighlighter.h"

#include <QGraphicsView>
#include <QString>
#include <QVector>
#include <memory>

class QGraphicsScene;

namespace Corbomite::ReadingView {

class ReadingPipeline;
class ReadingSection;
class SectionLayout;
class StyleManager;

/// Obsidian-compatible Reading-mode widget. Phase 3a wires in the
/// synchronous ReadingPipeline + SectionLayout: parse → section-split →
/// mount each section's QGraphicsItemGroup into the scene. Phase 4 adds
/// recycling; Phase 5 promotes ≥ 10240-byte parses onto a worker; Phase 6
/// adds virtualization.
class ReadingView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ReadingView(QWidget *parent = nullptr);
    ~ReadingView() override;

    /// Set the markdown source. Clears the scene, splits into sections,
    /// mounts each rendered section top-to-bottom.
    void setPlainText(const QString &markdown);

    /// Visual-line float scroll derived from mounted-section heights plus
    /// the pixel scroll offset. Width-change-stable because the position
    /// is expressed in visual lines (QFontMetricsF::lineSpacing()), not
    /// pixels.
    float scrollPositionVisualLine() const;
    void setScrollPositionVisualLine(float visualLine);

    /// Current body-content width used by SectionLayout for word-wrap.
    qreal contentWidth() const;
    void setContentWidth(qreal width);

    /// Active theme (maps to StyleManager palettes + CodeBlockHighlighter).
    Theme theme() const;
    void setTheme(Theme theme);

    /// Mounted sections — exposed for tests and Phase 4 recycling.
    const QVector<std::shared_ptr<ReadingSection>> &sections() const
    {
        return m_sections;
    }

Q_SIGNALS:
    void scrollPositionVisualLineChanged(float visualLine);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild();
    qreal visualLineSpacing() const;

    QString m_markdown;
    qreal m_contentWidth = 800.0;
    Theme m_theme = Theme::Light;

    std::unique_ptr<ReadingPipeline> m_pipeline;
    std::unique_ptr<SectionLayout> m_layout;
    std::unique_ptr<StyleManager> m_styles;

    QVector<std::shared_ptr<ReadingSection>> m_sections;
};

} // namespace Corbomite::ReadingView
