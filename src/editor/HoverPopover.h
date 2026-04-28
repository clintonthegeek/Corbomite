// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QPoint>
#include <QString>
#include <QTimer>

namespace Markoff::Reading {
class EmbedRenderer;
class ReadingView;
} // namespace Markoff::Reading

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
// Cluster J Phase 6 (2026-04-15) — content is rendered via
// `Markoff::Reading::EmbedRenderer` into an embedded
// `Markoff::Reading::ReadingView`. Math, mermaid, wiki-links, images,
// and nested embeds all Just Work in previews.
//
// 2026-04-27 — Mod-key pinning + 500 ms grace timer (P2 #4 sub-items 1+4).
// State machine is explicit; pinning is latching via Ctrl-press while
// Visible; replacement on `scheduleShow(newTarget)` always wins (single-
// popover invariant — child-chains are deferred sub-item 2). Spec:
// docs/superpowers/specs/2026-04-27-hoverpopover-pinning-grace-design.md
class HoverPopover : public QFrame {
    Q_OBJECT

public:
    enum class State {
        Hidden,   // not visible, no pending show
        Pending,  // 300ms delay timer running, will show on timeout
        Visible,  // shown, auto-dismissable (grace timer may be running)
        Pinned,   // shown, sticky — only Esc / outside-click / replacement
                  // dismisses
    };

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
    void setEmbedRenderer(Markoff::Reading::EmbedRenderer *renderer);

    // Schedule a popover for `target`. Cancels any pending show; if the
    // hover is still active 300ms later, the popover appears at `anchor`
    // (in global screen coordinates). When the popover is currently
    // Visible or Pinned with the same target, this is treated as a
    // "cursor returned to link" hint — stops the grace timer if running,
    // does not re-render. When the target differs, replacement wins
    // (Q3 = A): the popover hides and re-enters Pending with the new
    // (target, anchor).
    void scheduleShow(const QString &target, const QPoint &anchor);

    // Hard reset — cancels any pending show, clears Pinned state, hides
    // if visible. Used by vault close and other lifecycle teardowns.
    void cancel();

    // Soft hint that the source-link hover has ended. In Visible state,
    // starts the 500ms grace timer instead of dismissing immediately
    // (lets the cursor cross the gap from link to popover). In Pinned
    // state, no-op (pinned popovers ignore link-hover-end). In Pending
    // state, behaves as `cancel()`.
    void linkHoverEnded();

    // Test hook — current state.
    State stateForTest() const { return m_state; }
    bool isPinned() const { return m_state == State::Pinned; }

    // Test hook — returns the embedded ReadingView so integration tests
    // can assert the preview contains the expected sections / shapes.
    // Returns nullptr until the widget is fully constructed.
    Markoff::Reading::ReadingView *readingViewForTest() const;

protected:
    void leaveEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void enterState(State next);
    void showNow();
    void renderTarget(const QString &target);
    void onGraceTimeout();
    void applyPinnedAccent(bool on);

    Markoff::Reading::ReadingView *m_view = nullptr;
    Vault *m_vault = nullptr;
    Markoff::Reading::EmbedRenderer *m_embedRenderer = nullptr;
    QTimer m_delayTimer;
    QTimer m_graceTimer;
    QString m_pendingTarget;
    QString m_currentTarget;
    QPoint m_pendingAnchor;
    State m_state = State::Hidden;
    bool m_appFilterInstalled = false;
};

} // namespace Corbomite
