// src/app/MessageService.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QLabel;
class QMainWindow;
class QStatusBar;
class QWidget;

namespace Corbomite {

/// The app's one way to tell the user something happened.
///
/// Three sinks, chosen by severity and by whether the window is on screen:
///
///   · **every** message goes to the status bar's transient area — the same
///     place "Indexing vault…" goes — so an in-app message is always inside
///     the app window;
///   · **warning and error** also set a persistent alert entry beside the
///     word-count labels, so the message outlives the transient timeout
///     (click it to dismiss); and
///   · **warning and error** additionally go to the desktop as a notification
///     when the window is not active — the case a status-bar line cannot
///     cover, because nobody is looking at the status bar then.
///
/// This replaces Cluster H's floating `Notice` toast, which drew itself at the
/// screen's bottom-right — outside the window it was about, over whatever else
/// was on the desktop — and kept no history.
class MessageService : public QObject
{
    Q_OBJECT

public:
    enum class Severity {
        Info,     ///< status bar only
        Warning,  ///< status bar + alert entry + desktop notification when unfocused
        Error,    ///< as Warning, with the error icon
    };

    explicit MessageService(QMainWindow *host);

    /// Show `text`. `durationMs == 0` uses the severity's default (info 4000ms,
    /// warning/error 8000ms, matching Obsidian's 4s notice for the quiet case).
    void post(const QString &text, Severity severity = Severity::Info,
              int durationMs = 0);

    /// The alert entry's current text, or empty when nothing has gone wrong
    /// since it was last cleared. This is what the status bar keeps showing.
    QString alertText() const;

    /// How the desktop notification is delivered. The default sends through
    /// org.freedesktop.Notifications; tests and headless runs replace it (with a spy, or with
    /// nothing at all when there is no desktop).
    using NotificationSender = std::function<void(const QString &text, Severity severity)>;
    void setNotificationSender(NotificationSender sender);

    /// Whether the window counts as "in front of the user" — the condition that
    /// suppresses the desktop notification, because the status bar already says
    /// it. Defaults to `QMainWindow::isActiveWindow()`; injectable because an
    /// offscreen window is never active, so the suppressing half of the rule
    /// cannot otherwise be asserted.
    using ActivityProbe = std::function<bool()>;
    void setActivityProbe(ActivityProbe probe);

    /// The service owned by `context`'s window, else by the application's
    /// active window. Null when neither window has one — a widget living
    /// outside a MainWindow (a test, a headless tool).
    static MessageService *forContext(QWidget *context = nullptr);

    /// Post from `context`'s window, falling back to the active window, and to
    /// `qWarning()` when there is no window at all — so a message is never
    /// dropped silently just because the caller was headless.
    static void postFrom(QWidget *context, const QString &text,
                         Severity severity = Severity::Info,
                         int durationMs = 0);

    /// Post with no widget to hand (see `postFrom` for the resolution order).
    static void postToActiveWindow(const QString &text,
                                   Severity severity = Severity::Info,
                                   int durationMs = 0);

    /// `durationMs` for a severity when the caller does not say.
    static int defaultDurationMs(Severity severity);

protected:
    /// Clicking the alert entry dismisses it (it is a QLabel, not a button).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void sendDesktopNotification(const QString &text, Severity severity);

    QMainWindow *m_host = nullptr;
    QStatusBar *m_statusBar = nullptr;
    /// Persistent "last thing that went wrong", in the status bar's permanent
    /// area. Empty and hidden until a warning or error arrives.
    QLabel *m_alertLabel = nullptr;
    NotificationSender m_notify;
    ActivityProbe m_isActive;
};

} // namespace Corbomite
