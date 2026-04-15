// SPDX-License-Identifier: GPL-3.0-or-later
#include "HoverPopover.h"

#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/NoteService.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace Corbomite {

namespace {
constexpr int kHoverDelayMs = 300;   // audit: ui-bundle.md §7
constexpr int kPopoverWidth = 380;
constexpr int kPopoverMaxHeight = 280;
}

HoverPopover::HoverPopover(QWidget *parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);
    setFixedWidth(kPopoverWidth);
    setMaximumHeight(kPopoverMaxHeight);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 70));
    setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);

    m_browser = new QTextBrowser(this);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setOpenLinks(false);
    m_browser->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_browser);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(kHoverDelayMs);
    connect(&m_delayTimer, &QTimer::timeout, this, &HoverPopover::showNow);
}

void HoverPopover::setNoteService(NoteService *service)
{
    m_noteService = service;
}

void HoverPopover::scheduleShow(const QString &target, const QPoint &anchor)
{
    if (target.isEmpty()) {
        cancel();
        return;
    }
    m_pendingTarget = target;
    m_pendingAnchor = anchor;
    m_delayTimer.start();
}

void HoverPopover::cancel()
{
    m_delayTimer.stop();
    m_pendingTarget.clear();
    if (isVisible()) hide();
}

void HoverPopover::showNow()
{
    if (m_pendingTarget.isEmpty()) return;
    renderTarget(m_pendingTarget);
    move(m_pendingAnchor);
    show();
    raise();
}

void HoverPopover::renderTarget(const QString &target)
{
    if (!m_noteService) {
        m_browser->setPlainText(target);
        return;
    }
    auto *doc = m_noteService->openNote(target);
    if (!doc) {
        m_browser->setPlainText(QStringLiteral("(unresolved: %1)").arg(target));
        return;
    }
    // Qt's built-in markdown renderer is adequate for a small preview; the
    // full Markoff::ReadingView replacement is spec'd in
    // libs/markoff/docs/specs/2026-04-02-markoff-public-api-design.md.
    m_browser->setMarkdown(doc->markdown());
}

void HoverPopover::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    cancel();
}

void HoverPopover::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    QFrame::keyPressEvent(event);
}

} // namespace Corbomite
