// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QPoint>
#include <QString>
#include <QTimer>

namespace Corbomite::ReadingView {
class EmbedRenderer;
class ReadingView;
} // namespace Corbomite::ReadingView

namespace Corbomite {

class Vault;

// Floating preview that pops up after a 300ms hover over a wiki/markdown
// link. Per docs/obsidian-audit/domains/ui-bundle.md §7 — the delay constant
// is 300ms exactly (not 500ms; that's the registry poll interval).
//
// Three-wire architecture (audit §7):
//   (1) HoverLinkSourceRegistry says whether this view is allowed to fire,
//   (2) the hover-link signal carries (sourceId, target, anchor),
//   (3) HoverPopover (this widget) mounts at the cursor and renders content.
//
// Cluster J Phase 6 (2026-04-15) — content is now rendered via
// `Corbomite::ReadingView::EmbedRenderer` into an embedded
// `Corbomite::ReadingView::ReadingView`. Math, mermaid, wiki-links, images,
// and nested embeds all Just Work in previews. Anchoring is still
// `QCursor::pos()`; rect-based anchoring (`hoveredLinkRect()`) is a named
// Markoff-API follow-up outside Cluster J's scope.
class HoverPopover : public QFrame {
    Q_OBJECT

public:
    explicit HoverPopover(QWidget *parent = nullptr);
    ~HoverPopover() override;

    // Set the canonical Vault used to look up target paths → markdown
    // bodies via the legacy fallback path when no EmbedRenderer is wired.
    void setVault(Vault *vault);

    // Cluster J Phase 6 — supply the per-vault `EmbedRenderer`. When set,
    // `renderTarget` routes through `EmbedRenderer::render` so math /
    // mermaid / wiki-links / images / nested embeds all render in the
    // preview. Caller retains ownership; pass `nullptr` to clear (e.g.,
    // on vault close).
    void setEmbedRenderer(Corbomite::ReadingView::EmbedRenderer *renderer);

    // Schedule a popover for `target`. Cancels any pending show; if the
    // hover is still active 300ms later, the popover appears at `anchor`
    // (in global screen coordinates).
    void scheduleShow(const QString &target, const QPoint &anchor);

    // Cancel any pending show and hide the popover if visible. Called when
    // the hover ends (`linkHovered("")`) or when the user moves the cursor
    // away from the popover.
    void cancel();

    // Test hook — returns the embedded ReadingView so integration tests
    // can assert the preview contains the expected sections / shapes.
    // Returns nullptr until the widget is fully constructed.
    Corbomite::ReadingView::ReadingView *readingViewForTest() const;

protected:
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void showNow();
    void renderTarget(const QString &target);

    Corbomite::ReadingView::ReadingView *m_view = nullptr;
    Vault *m_vault = nullptr;
    Corbomite::ReadingView::EmbedRenderer *m_embedRenderer = nullptr;
    QTimer m_delayTimer;
    QString m_pendingTarget;
    QPoint m_pendingAnchor;
};

} // namespace Corbomite
