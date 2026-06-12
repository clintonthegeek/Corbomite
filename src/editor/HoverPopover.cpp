// SPDX-License-Identifier: GPL-3.0-or-later
#include "HoverPopover.h"

#include "corbomite/core/MarkdownRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/VaultResourceProvider.h"

#include <QTextBrowser>
#include <QTextDocument>

#include <optional>

#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace Corbomite {

namespace {
constexpr int kHoverDelayMs = 300;   // audit: ui-bundle.md §7
constexpr int kGracePeriodMs = 500;  // audit: ui-bundle.md §HoverPopover #3
constexpr int kPopoverWidth = 380;
constexpr int kPopoverMaxHeight = 280;

/// Split a hover-link target into `(path, subpath)`. The first `#`
/// character separates the file path from the wikilink subpath; the
/// `#` is preserved on the subpath portion so callers can detect
/// `#^block` vs `#heading` discriminator. Mirrors the
/// `splitWikiEmbed` shape inside `EmbedRenderer.cpp`.
void splitTarget(const QString &raw, QString *path, QString *subpath)
{
    const int hash = raw.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        *path = raw.left(hash);
        *subpath = raw.mid(hash);
    } else {
        *path = raw;
        subpath->clear();
    }
}

// True when `point` (global screen coords) falls inside `popover`'s frame
// or the frame of any descendant. Walks the parent chain from the widget
// returned by `QApplication::widgetAt` so child widgets (the embedded
// QTextBrowser, scroll bars, etc.) count as "inside".
bool cursorIsOverPopover(const QWidget *popover, const QPoint &globalPos)
{
    if (!popover || !popover->isVisible()) return false;
    if (popover->frameGeometry().contains(globalPos)) return true;
    const QWidget *under = QApplication::widgetAt(globalPos);
    while (under) {
        if (under == popover) return true;
        under = under->parentWidget();
    }
    return false;
}
} // namespace

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

    m_display = new QTextBrowser(this);
    m_display->setOpenLinks(false);
    m_display->setOpenExternalLinks(false);
    m_display->setReadOnly(true);
    m_display->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_display);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(kHoverDelayMs);
    connect(&m_delayTimer, &QTimer::timeout, this, &HoverPopover::showNow);

    m_graceTimer.setSingleShot(true);
    m_graceTimer.setInterval(kGracePeriodMs);
    connect(&m_graceTimer, &QTimer::timeout, this,
            &HoverPopover::onGraceTimeout);
}

HoverPopover::~HoverPopover()
{
    if (m_appFilterInstalled && qApp) {
        qApp->removeEventFilter(this);
        m_appFilterInstalled = false;
    }
}

void HoverPopover::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
}

void HoverPopover::setResources(Core::VaultResourceProvider *resources)
{
    m_resources = resources;
}

QString HoverPopover::previewPlainText() const
{
    return m_display ? m_display->toPlainText() : QString();
}

void HoverPopover::scheduleShow(const QString &target, const QPoint &anchor)
{
    if (target.isEmpty()) {
        linkHoverEnded();
        return;
    }

    // "Cursor returned to link" hint — same target, popover already up.
    // Stop grace timer (if running) and stay; don't re-render.
    if ((m_state == State::Visible || m_state == State::Pinned)
        && target == m_currentTarget) {
        m_graceTimer.stop();
        return;
    }

    // Different target while shown — replacement wins (Q3 = A: even when
    // pinned, a new hover spawns a new popover; the previous one is
    // dropped). Hide current, re-enter Pending with the new (t, a).
    m_pendingTarget = target;
    m_pendingAnchor = anchor;
    if (m_state == State::Visible || m_state == State::Pinned) {
        enterState(State::Hidden);
    }
    enterState(State::Pending);
}

void HoverPopover::cancel()
{
    m_pendingTarget.clear();
    enterState(State::Hidden);
}

void HoverPopover::linkHoverEnded()
{
    switch (m_state) {
    case State::Hidden:
        return;
    case State::Pending:
        m_pendingTarget.clear();
        enterState(State::Hidden);
        return;
    case State::Visible:
        // Start the grace timer instead of hiding immediately. Lets the
        // cursor cross the gap from source link to popover without the
        // popover tearing down mid-traverse.
        if (!m_graceTimer.isActive()) m_graceTimer.start();
        return;
    case State::Pinned:
        // Pinned popovers ignore link-hover-end — only Esc, outside
        // click, or replacement dismisses them.
        return;
    }
}

void HoverPopover::enterState(State next)
{
    if (m_state == next) return;
    const State prev = m_state;
    m_state = next;

    switch (next) {
    case State::Hidden:
        m_delayTimer.stop();
        m_graceTimer.stop();
        if (prev == State::Pinned) applyPinnedAccent(false);
        if (isVisible()) hide();
        m_currentTarget.clear();
        if (m_appFilterInstalled && qApp) {
            qApp->removeEventFilter(this);
            m_appFilterInstalled = false;
        }
        break;
    case State::Pending:
        m_graceTimer.stop();
        if (prev == State::Pinned) applyPinnedAccent(false);
        if (isVisible()) hide();
        m_delayTimer.start();
        break;
    case State::Visible:
        m_delayTimer.stop();
        // Filter is already installed if we came from Pinned; install on
        // the Pending → Visible transition.
        if (!m_appFilterInstalled && qApp) {
            qApp->installEventFilter(this);
            m_appFilterInstalled = true;
        }
        if (prev == State::Pinned) applyPinnedAccent(false);
        break;
    case State::Pinned:
        m_graceTimer.stop();
        applyPinnedAccent(true);
        break;
    }
}

void HoverPopover::showNow()
{
    if (m_state != State::Pending) return;
    if (m_pendingTarget.isEmpty()) {
        enterState(State::Hidden);
        return;
    }
    m_currentTarget = m_pendingTarget;
    renderTarget(m_currentTarget);
    move(m_pendingAnchor);
    show();
    raise();
    enterState(State::Visible);
}

void HoverPopover::renderTarget(const QString &target)
{
    if (!m_display) return;

    QString path;
    QString subpath;
    splitTarget(target, &path, &subpath);

    const std::optional<QString> md =
        m_resources ? m_resources->resolveEmbed(path) : std::nullopt;
    // Subpath (#heading / #^block) slicing is deferred — render whole note.
    const QString markdown =
        md ? *md
           : QStringLiteral("*(unresolved: %1)*").arg(target);

    if (m_renderEngine) {
        const auto rendered = m_renderEngine->render(markdown);
        if (rendered && rendered->toQTextDocument()) {
            // Copy the styled content into the browser's own document
            // (ownership-safe; mirrors RenderedDocument::createWidget()).
            m_display->setHtml(rendered->toQTextDocument()->toHtml());
            return;
        }
    }
    // Defensive fallback: no engine wired — show raw markdown.
    m_display->setPlainText(markdown);
}

void HoverPopover::onGraceTimeout()
{
    if (m_state != State::Visible) return;
    if (cursorIsOverPopover(this, QCursor::pos())) {
        // Safety net: if the cursor sits inside us but enterEvent didn't
        // fire (synthetic-event paths), keep visible.
        return;
    }
    enterState(State::Hidden);
}

void HoverPopover::applyPinnedAccent(bool on)
{
    if (on) {
        setStyleSheet(QStringLiteral("QFrame { border: 2px solid %1; }")
                          .arg(palette().highlight().color().name()));
    } else {
        setStyleSheet(QString());
    }
}

void HoverPopover::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    if (m_state == State::Visible && !m_graceTimer.isActive()) {
        m_graceTimer.start();
    }
}

void HoverPopover::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    m_graceTimer.stop();
}

void HoverPopover::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    QFrame::keyPressEvent(event);
}

bool HoverPopover::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    switch (event->type()) {
    case QEvent::KeyPress: {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        // Esc dismisses from any state where the popover is up — the
        // local keyPressEvent override doesn't fire because the popover
        // doesn't accept focus, so the app filter is the only path.
        if (keyEvent->key() == Qt::Key_Escape
            && (m_state == State::Visible || m_state == State::Pinned)) {
            cancel();
            break;
        }
        // Linux convention — Ctrl is the modifier that "grabs" the
        // popover for interaction. (Mac would map to Qt::Key_Meta; out
        // of scope for this single-platform decision.)
        if (m_state == State::Visible
            && keyEvent->key() == Qt::Key_Control) {
            enterState(State::Pinned);
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        if (m_state != State::Pinned) break;
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!frameGeometry().contains(mouseEvent->globalPosition().toPoint())) {
            enterState(State::Hidden);
        }
        break;
    }
    default:
        break;
    }
    // Never consume — observation only.
    return false;
}

} // namespace Corbomite
