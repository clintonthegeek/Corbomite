// SPDX-License-Identifier: GPL-3.0-or-later
//
// Punch list [LIVE-FOUND 2026-10-18]: the status bar kept the last text
// view's word/line readouts when the focused leaf is not a text view (a
// graph tab read a markdown note's counts, frozen; earlier a canvas tab
// read "Words: 1,606"). The labels belong to a text view; a non-text leaf
// shows nothing rather than the previous note's numbers.
//
// Driven through a real MainWindow + vault, same pattern as
// tst_mainwindow_link_navigation / tst_action_context. Offscreen.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"

using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

const QString kCanvasSeed = QStringLiteral(
    "{\"nodes\":[{\"id\":\"n1\",\"type\":\"text\",\"x\":0,\"y\":0,"
    "\"width\":250,\"height\":60,\"text\":\"hi\"}],\"edges\":[]}");

// The two permanent readouts are QLabel children; pick them by their
// known placeholder text so the test never depends on member order.
QLabel *wordsLabel(MainWindow *mw)
{
    for (QLabel *l : mw->findChildren<QLabel *>())
        if (l->text().startsWith(QStringLiteral("Words:"))) return l;
    return nullptr;
}

QLabel *cursorLabel(MainWindow *mw)
{
    for (QLabel *l : mw->findChildren<QLabel *>())
        if (l->text().startsWith(QStringLiteral("Ln"))) return l;
    return nullptr;
}

} // namespace

class TstStatusBarCounts : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("status bar test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void nonTextLeafClearsTheReadouts()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // Enough words that the count is visibly != 0.
        createFile(tmp.path() + QStringLiteral("/Note.md"),
                   QStringLiteral("alpha beta gamma delta epsilon zeta"));
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        mw.show();
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(300);

        // A markdown note focused: the labels carry real counts.
        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(300);
        QLabel *words = wordsLabel(&mw);
        QLabel *cursor = cursorLabel(&mw);
        QVERIFY(words);
        QVERIFY(cursor);
        QVERIFY2(words->text() != QStringLiteral("Words: 0"),
                 qPrintable(words->text()));

        // A canvas leaf focused: both readouts reset — a canvas shows its
        // own information (or nothing), never the previous note's numbers.
        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(300);
        QCOMPARE(words->text(), QStringLiteral("Words: 0"));
        QCOMPARE(cursor->text(), QStringLiteral("Ln 1, Col 1"));

        // Back to the note: the widget re-seeds on activation (showEvent →
        // refreshWordCount), and the words count is the visible proof that
        // the clear didn't leave a frozen zero. The cursor field legitimately
        // reads "Ln 1, Col 1" for a never-focused note, so only words
        // distinguishes re-seeded from cleared.
        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(300);
        QVERIFY2(words->text() != QStringLiteral("Words: 0"),
                 qPrintable(words->text()));
    }
};

QTEST_MAIN(TstStatusBarCounts)
#include "tst_status_bar_counts.moc"
