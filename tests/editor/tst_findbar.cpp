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

QTEST_MAIN(TstFindBar)
#include "tst_findbar.moc"
