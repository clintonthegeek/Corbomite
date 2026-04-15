// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/readingview/CodeBlockHighlighter.h"

#include <QGraphicsView>
#include <QString>
#include <QVector>
#include <memory>

class QGraphicsScene;
class QTimer;

namespace Corbomite::ReadingView {

class ReadingPipeline;
class ReadingSection;
class SectionLayout;
class SectionRecyclePool;
class StyleManager;
class VaultResourceProvider;

/// Obsidian-compatible Reading-mode widget. Phase 3b wires in eleven
/// content types — headings, paragraphs, code blocks, lists, horizontal
/// rules, blockquotes, tables, inline images, wiki-links, math (inline +
/// display), and Mermaid fenced blocks.
///
/// Wiki-link activation:
/// - a click inside a wiki-link fragment emits `wikiLinkActivated(target)`.
/// - hover inside a wiki-link fragment emits `wikiLinkHovered(target)`
///   debounced at 300 ms.
class ReadingView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ReadingView(QWidget *parent = nullptr);
    ~ReadingView() override;

    void setPlainText(const QString &markdown);

    float scrollPositionVisualLine() const;
    void setScrollPositionVisualLine(float visualLine);

    qreal contentWidth() const;
    void setContentWidth(qreal width);

    Theme theme() const;
    void setTheme(Theme theme);

    /// Supply a vault resource provider for image embeds + wiki-link
    /// resolution. The caller retains ownership; pass `nullptr` to clear.
    void setVaultResourceProvider(VaultResourceProvider *provider);
    VaultResourceProvider *vaultResourceProvider() const;

    const QVector<std::shared_ptr<ReadingSection>> &sections() const
    {
        return m_sections;
    }

    /// Pool size — exposed for tests and diagnostics. Phase 4 adds no
    /// eviction policy beyond "first-in wins"; size grows with unique
    /// discarded shapes until `clear()` or dtor.
    int recyclePoolSize() const;

Q_SIGNALS:
    void scrollPositionVisualLineChanged(float visualLine);
    void wikiLinkActivated(const QString &target);
    void wikiLinkHovered(const QString &target);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void rebuild();
    qreal visualLineSpacing() const;
    QString wikiLinkTargetAt(const QPoint &viewportPos) const;

    QString m_markdown;
    QString m_lastMarkdown;
    qreal m_contentWidth = 800.0;
    Theme m_theme = Theme::Light;
    VaultResourceProvider *m_vaultProvider = nullptr;

    std::unique_ptr<ReadingPipeline> m_pipeline;
    std::unique_ptr<SectionLayout> m_layout;
    std::unique_ptr<StyleManager> m_styles;
    std::unique_ptr<SectionRecyclePool> m_recyclePool;

    QVector<std::shared_ptr<ReadingSection>> m_sections;

    QTimer *m_hoverTimer = nullptr;
    QString m_pendingHoverTarget;
};

} // namespace Corbomite::ReadingView
