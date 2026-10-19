// SPDX-License-Identifier: GPL-3.0-or-later
//
// MessageService — the app's one user-message channel, added 2026-10-18 to
// replace Cluster H's floating `Notice` toast. That toast drew itself at the
// screen's bottom-right: outside the window it was about, over whatever else
// happened to be on the desktop, with no history. The replacement has three
// sinks and this file pins which one each severity reaches:
//
//   · the status bar's transient area — everything;
//   · a persistent alert entry in the status bar — warning and error; and
//   · the desktop notification daemon — warning and error **only while the
//     window is not in front of the user**, since a status-bar line is
//     useless to someone who is looking at another application.
//
// The desktop transport and the activity check are both injectable, so the
// half of that rule that only applies when the app is unfocused is asserted
// rather than assumed (an offscreen window is never active).
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include <QTest>

#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>

#include "app/MessageService.h"

using Corbomite::MessageService;

namespace {

/// A MainWindow with just enough around it to host the service.
class HostWindow : public QMainWindow
{
public:
    HostWindow() { m_messages = new MessageService(this); }
    MessageService *messages() const { return m_messages; }

private:
    MessageService *m_messages = nullptr;
};

/// The alert entry the service adds to the status bar's permanent area.
QLabel *alertLabel(HostWindow &host)
{
    return host.findChild<QLabel *>(QStringLiteral("messageAlert"));
}

/// True when the alert entry is set to be shown. `isVisibleTo` rather than
/// `isVisible`: the host window is deliberately never shown in these cases, and
/// a child of an unshown window reports isVisible() false regardless.
bool alertRaised(HostWindow &host)
{
    QLabel *label = alertLabel(host);
    return label && label->isVisibleTo(&host);
}

} // namespace

class TestMessageService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void infoReachesTheStatusBarOnly();
    void warningAlsoRaisesTheAlertEntry();
    void focusedWindowSkipsTheDesktopNotification();
    void unfocusedWindowNotifiesTheDesktop();
    void clickingTheAlertDismissesIt();
    void postFromWithoutAWindowStillReports();
};

void TestMessageService::infoReachesTheStatusBarOnly()
{
    HostWindow host;
    int notifications = 0;
    host.messages()->setNotificationSender([&notifications](const QString &, MessageService::Severity) {
        ++notifications;
    });

    host.messages()->post(QStringLiteral("Indexing complete"), MessageService::Severity::Info);

    QVERIFY2(host.statusBar()->currentMessage().contains(QStringLiteral("Indexing complete")),
             "info must reach the status bar's transient area");
    QCOMPARE(notifications, 0);
    QVERIFY2(!alertRaised(host),
             "a plain info message must not raise the persistent alert entry");
}

void TestMessageService::warningAlsoRaisesTheAlertEntry()
{
    HostWindow host;
    host.messages()->setNotificationSender({});

    host.messages()->post(QStringLiteral("Keeping your version"), MessageService::Severity::Warning);

    QVERIFY(host.statusBar()->currentMessage().contains(QStringLiteral("Keeping your version")));
    QVERIFY2(alertRaised(host),
             "a warning must stay visible after the transient message times out");
    QCOMPARE(host.messages()->alertText(), QStringLiteral("Keeping your version"));
}

void TestMessageService::focusedWindowSkipsTheDesktopNotification()
{
    HostWindow host;
    int notifications = 0;
    host.messages()->setNotificationSender([&notifications](const QString &, MessageService::Severity) {
        ++notifications;
    });
    // The window is in front of the user: the status bar already says it.
    host.messages()->setActivityProbe([] { return true; });

    host.messages()->post(QStringLiteral("Quietly handled"), MessageService::Severity::Error);

    QCOMPARE(notifications, 0);
    QVERIFY(alertRaised(host));
}

void TestMessageService::unfocusedWindowNotifiesTheDesktop()
{
    HostWindow host;
    QString notified;
    MessageService::Severity notifiedSeverity = MessageService::Severity::Info;
    host.messages()->setNotificationSender(
        [&notified, &notifiedSeverity](const QString &text, MessageService::Severity severity) {
            notified = text;
            notifiedSeverity = severity;
        });
    host.messages()->setActivityProbe([] { return false; });

    host.messages()->post(QStringLiteral("Changed on disk"), MessageService::Severity::Warning);

    QCOMPARE(notified, QStringLiteral("Changed on disk"));
    QCOMPARE(notifiedSeverity, MessageService::Severity::Warning);
    // And it is still an in-app message as well — the desktop copy is an
    // addition, never the only place the message lands.
    QVERIFY(host.statusBar()->currentMessage().contains(QStringLiteral("Changed on disk")));
}

void TestMessageService::clickingTheAlertDismissesIt()
{
    HostWindow host;
    host.messages()->setNotificationSender({});

    host.messages()->post(QStringLiteral("Something went wrong"), MessageService::Severity::Error);
    QVERIFY(alertRaised(host));

    QTest::mouseClick(alertLabel(host), Qt::LeftButton);
    QVERIFY2(alertLabel(host)->isHidden(), "clicking the alert entry dismisses it");
    QVERIFY(host.messages()->alertText().isEmpty());
}

void TestMessageService::postFromWithoutAWindowStillReports()
{
    // No MainWindow, so no status bar to write to: the message must still be
    // reported rather than dropped (this is the headless/test path).
    QTest::ignoreMessage(QtWarningMsg, "nothing to draw into");
    MessageService::postFrom(nullptr, QStringLiteral("nothing to draw into"),
                             MessageService::Severity::Warning);
}

QTEST_MAIN(TestMessageService)
#include "tst_messageservice.moc"
