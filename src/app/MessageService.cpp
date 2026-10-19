// src/app/MessageService.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MessageService.h"

#include <KLocalizedString>

#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QToolTip>

namespace Corbomite {

namespace {

/// Information is transient only; anything that went wrong stays visible.
constexpr int kInfoMs = 4000;
constexpr int kWarningMs = 8000;

} // namespace

MessageService::MessageService(QMainWindow *host)
    : QObject(host)
    , m_host(host)
    , m_statusBar(host ? host->statusBar() : nullptr)
{
    if (m_host) {
        // Permanent, so it sits beside the word-count / cursor labels and does
        // not fight the transient message area.
        m_alertLabel = new QLabel(m_host);
        m_alertLabel->setObjectName(QStringLiteral("messageAlert"));
        m_alertLabel->setVisible(false);
        m_alertLabel->setToolTip(i18n("Click to dismiss"));
        m_alertLabel->setCursor(Qt::PointingHandCursor);
        // QLabel is not a button; filter for the click so the label stays
        // plain text in the status bar.
        m_alertLabel->installEventFilter(this);
        m_statusBar->addPermanentWidget(m_alertLabel);
    }

    // Desktop notification transport: a direct org.freedesktop.Notifications
    // Notify call.
    //
    // KNotification would be the framework-shaped way to do this, and it is
    // what this used first. It does not work here: with
    // KNotificationPermission::checkPermission() reporting Granted, our
    // sendEvent() produced **no** Notify call on the session bus (verified
    // 2026-10-18 with dbus-monitor while an identical `notify-send` call in the
    // same window did arrive, and the daemon showed its popup). The dev build
    // has no installed desktop file for the daemon to attribute the
    // notification to — punch-list P6 — and KNotification swallows that
    // silently. The raw call is what notify-send itself does, and the daemon
    // still owns do-not-disturb, history and appearance, so nothing is lost
    // except the wrapper around a writer that does not fire.
    //
    // asyncCall, not QDBusInterface::call: the latter blocks up to the reply
    // timeout, and this runs from a signal handler on the GUI thread.
    m_notify = [](const QString &text, Severity severity) {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("Notify"));
        QVariantMap hints;
        hints.insert(QStringLiteral("desktop-entry"),
                     QGuiApplication::desktopFileName().isEmpty()
                         ? QCoreApplication::applicationName()
                         : QGuiApplication::desktopFileName());
        // Normal urgency for both severities: whether a message may interrupt
        // is the user's setting (do-not-disturb), not ours to override.
        hints.insert(QStringLiteral("urgency"), 1);
        message << QCoreApplication::applicationName()  // app name
                << 0u                                   // replaces id
                << (severity == Severity::Error ? QStringLiteral("dialog-error")
                                                : QStringLiteral("dialog-warning"))
                << i18n("Corbomite")  // summary
                << text               // body
                << QStringList{}      // actions
                << hints
                << -1;                // no timeout hint: the daemon decides
        QDBusConnection::sessionBus().asyncCall(message);
    };
}

int MessageService::defaultDurationMs(Severity severity)
{
    return severity == Severity::Info ? kInfoMs : kWarningMs;
}

void MessageService::post(const QString &text, Severity severity, int durationMs)
{
    if (text.isEmpty())
        return;

    const int ms = durationMs > 0 ? durationMs : defaultDurationMs(severity);

    if (m_statusBar)
        m_statusBar->showMessage(text, ms);

    if (severity == Severity::Info)
        return;

    if (m_alertLabel) {
        m_alertLabel->setPixmap(QIcon::fromTheme(severity == Severity::Error
                                                    ? QStringLiteral("dialog-error")
                                                    : QStringLiteral("dialog-warning"))
                                    .pixmap(14, 14));
        // The status bar is one line; the full text lives in the tooltip.
        QString shortText = text;
        if (shortText.size() > 60)
            shortText = shortText.left(57) + QStringLiteral("…");
        m_alertLabel->setText(shortText);
        m_alertLabel->setToolTip(text);
        m_alertLabel->setVisible(true);
    }

    // A status-bar line is only useful to someone looking at the window. A
    // minimised or hidden window counts as out of sight even when the window
    // manager still reports it active — which is what XWayland does.
    const bool windowInFront = m_isActive
        ? m_isActive()
        : (m_host && m_host->isActiveWindow() && !m_host->isMinimized()
           && m_host->isVisible());
    if (!windowInFront && m_notify)
        sendDesktopNotification(text, severity);
}

QString MessageService::alertText() const
{
    // isHidden(), not isVisible(): a child of a window that is not on screen
    // (minimised, or never shown) reports isVisible() false even while it is
    // set to be shown, which would silently blank the alert text.
    if (!m_alertLabel || m_alertLabel->isHidden())
        return {};
    return m_alertLabel->toolTip();
}

void MessageService::setNotificationSender(NotificationSender sender)
{
    m_notify = std::move(sender);
}

void MessageService::setActivityProbe(ActivityProbe probe)
{
    m_isActive = std::move(probe);
}

bool MessageService::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_alertLabel && event->type() == QEvent::MouseButtonRelease) {
        m_alertLabel->clear();
        m_alertLabel->setVisible(false);
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void MessageService::sendDesktopNotification(const QString &text, Severity severity)
{
    m_notify(text, severity);
}

MessageService *MessageService::forContext(QWidget *context)
{
    QWidget *window = context ? context->window() : QApplication::activeWindow();
    if (!window)
        return nullptr;
    return window->findChild<MessageService *>();
}

void MessageService::postFrom(QWidget *context, const QString &text, Severity severity,
                              int durationMs)
{
    if (auto *service = forContext(context)) {
        service->post(text, severity, durationMs);
        return;
    }
    // No window to draw into: keep the message rather than dropping it.
    qWarning().noquote() << text;
}

void MessageService::postToActiveWindow(const QString &text, Severity severity, int durationMs)
{
    postFrom(nullptr, text, severity, durationMs);
}

} // namespace Corbomite
