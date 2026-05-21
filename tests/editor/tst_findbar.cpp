// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/FindController.h>
#include <markoff/core/Origin.h>

#include "FindBar.h"

using namespace Corbomite;

class TstFindBar : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void unbound_safe();
    void textChanged_updatesController();
    void matchesChanged_updatesCountLabel();
    void noMatch_showsNoMatches();
    void prevNextButton_disabledWhenNoMatches();
};

void TstFindBar::unbound_safe()
{
    FindBar bar;
    auto *lineEdit  = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *countLabel = bar.findChild<QLabel*>("findBarCountLabel");
    auto *prevBtn   = bar.findChild<QPushButton*>("findBarPrev");
    auto *nextBtn   = bar.findChild<QPushButton*>("findBarNext");
    auto *closeBtn  = bar.findChild<QToolButton*>("findBarClose");

    QVERIFY(lineEdit);
    QVERIFY(countLabel);
    QVERIFY(prevBtn);
    QVERIFY(nextBtn);
    QVERIFY(closeBtn);

    QCOMPARE(countLabel->text(), QString());
    QVERIFY(!prevBtn->isEnabled());
    QVERIFY(!nextBtn->isEnabled());
    QVERIFY(closeBtn->isEnabled());
    QCOMPARE(bar.controller(), nullptr);
}

void TstFindBar::textChanged_updatesController()
{
    Markoff::MarkoffDocument doc(1);
    doc.resetContent(QByteArray("Hello world\n"), Markoff::Origin::FirstOpen);
    Markoff::FindController fc(&doc);

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    QVERIFY(lineEdit);

    lineEdit->setText(QStringLiteral("Hello"));

    QCOMPARE(fc.needle(), QStringLiteral("Hello"));
}

void TstFindBar::matchesChanged_updatesCountLabel()
{
    Markoff::MarkoffDocument doc(1);
    // "test" appears 3 times.
    doc.loadFromMarkdown(QByteArray("test alpha test beta test\n"));
    Markoff::FindController fc(&doc);
    fc.activate();  // Host activates before showing the bar (see Task 7 showFindBar).

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit  = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *countLabel = bar.findChild<QLabel*>("findBarCountLabel");

    lineEdit->setText(QStringLiteral("test"));

    QCOMPARE(fc.matchCount(), 3);
    QCOMPARE(countLabel->text(), QStringLiteral("1 of 3"));
}

void TstFindBar::noMatch_showsNoMatches()
{
    Markoff::MarkoffDocument doc(1);
    doc.loadFromMarkdown(QByteArray("hello\n"));
    Markoff::FindController fc(&doc);
    fc.activate();  // Host activates before showing the bar (see Task 7 showFindBar).

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit  = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *countLabel = bar.findChild<QLabel*>("findBarCountLabel");

    lineEdit->setText(QStringLiteral("zzz"));

    QCOMPARE(fc.matchCount(), 0);
    QCOMPARE(countLabel->text(), QStringLiteral("No matches"));
}

void TstFindBar::prevNextButton_disabledWhenNoMatches()
{
    Markoff::MarkoffDocument doc(1);
    doc.loadFromMarkdown(QByteArray("hello\n"));
    Markoff::FindController fc(&doc);
    fc.activate();  // Host activates before showing the bar (see Task 7 showFindBar).

    FindBar bar;
    bar.setController(&fc);
    auto *lineEdit = bar.findChild<QLineEdit*>("findBarLineEdit");
    auto *prevBtn  = bar.findChild<QPushButton*>("findBarPrev");
    auto *nextBtn  = bar.findChild<QPushButton*>("findBarNext");

    // Empty needle: disabled.
    QVERIFY(!prevBtn->isEnabled());
    QVERIFY(!nextBtn->isEnabled());

    // No-match needle: still disabled.
    lineEdit->setText(QStringLiteral("zzz"));
    QVERIFY(!prevBtn->isEnabled());
    QVERIFY(!nextBtn->isEnabled());

    // Match: enabled.
    lineEdit->setText(QStringLiteral("hello"));
    QVERIFY(prevBtn->isEnabled());
    QVERIFY(nextBtn->isEnabled());
}

QTEST_MAIN(TstFindBar)
#include "tst_findbar.moc"
