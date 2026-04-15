// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QPoint>
#include <QString>
#include <QTimer>

class QTextBrowser;

namespace Corbomite {

class NoteService;

// Floating preview that pops up after a 300ms hover over a wiki/markdown
// link. Per docs/obsidian-audit/domains/ui-bundle.md §7 — the delay constant
// is 300ms exactly (not 500ms; that's the registry poll interval).
//
// Three-wire architecture (audit §7):
//   (1) HoverLinkSourceRegistry says whether this view is allowed to fire,
//   (2) the hover-link signal carries (sourceId, target, anchor),
//   (3) HoverPopover (this widget) mounts at the cursor and renders content.
//
// Content is rendered into a QTextBrowser via Qt's built-in
// QTextDocument::setMarkdown — adequate for a small preview. Future work
// (Cluster J + the Markoff::ReadingView spec) replaces the text browser
// with a full Markoff::ReadingView.
class HoverPopover : public QFrame {
    Q_OBJECT

public:
    explicit HoverPopover(QWidget *parent = nullptr);

    // Set the NoteService used to look up target paths → markdown bodies.
    // Required before showForLink() can render anything.
    void setNoteService(NoteService *service);

    // Schedule a popover for `target`. Cancels any pending show; if the
    // hover is still active 300ms later, the popover appears at `anchor`
    // (in global screen coordinates).
    void scheduleShow(const QString &target, const QPoint &anchor);

    // Cancel any pending show and hide the popover if visible. Called when
    // the hover ends (`linkHovered("")`) or when the user moves the cursor
    // away from the popover.
    void cancel();

protected:
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void showNow();
    void renderTarget(const QString &target);

    QTextBrowser *m_browser = nullptr;
    NoteService *m_noteService = nullptr;
    QTimer m_delayTimer;
    QString m_pendingTarget;
    QPoint m_pendingAnchor;
};

} // namespace Corbomite
