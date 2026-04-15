// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/readingview/CodeBlockHighlighter.h"

#include <QGraphicsView>
#include <QMultiHash>
#include <QString>
#include <QVector>
#include <memory>

class QGraphicsScene;
class QTimer;

namespace Corbomite::ReadingView {

class ReadingParseWorker;
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
    /// Emitted when every section of the most recent parse has been mounted
    /// into the scene. Fires at the end of the frame-budgeted mount loop.
    void mountingFinished();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void rebuild();
    void beginMount(QVector<std::shared_ptr<ReadingSection>> newSections);
    void mountSectionsWithBudget(int startIdx);
    void onParseFinished(quint64 requestId,
                         QVector<std::shared_ptr<ReadingSection>> sections);
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
    std::unique_ptr<ReadingParseWorker> m_worker;

    QVector<std::shared_ptr<ReadingSection>> m_sections;

    // Mount-loop state — lives across frame yields. A fresh mount run
    // resets these in `beginMount`.
    QVector<std::shared_ptr<ReadingSection>> m_pendingSections;
    QVector<std::shared_ptr<ReadingSection>> m_mountedSoFar;
    QMultiHash<QByteArray, std::shared_ptr<ReadingSection>> m_oldByShape;
    qreal m_mountY = 0.0;
    bool m_pendingFmChanged = false;
    bool m_mountInProgress = false;

    // Coalescing against stale parseFinished arrivals. Every setPlainText
    // call bumps `m_requestIdCounter`; parseFinished handlers ignore any
    // requestId older than `m_lastRequestIdHandled`.
    quint64 m_requestIdCounter = 0;
    quint64 m_lastRequestIdHandled = 0;

    QTimer *m_hoverTimer = nullptr;
    QString m_pendingHoverTarget;
};

} // namespace Corbomite::ReadingView
