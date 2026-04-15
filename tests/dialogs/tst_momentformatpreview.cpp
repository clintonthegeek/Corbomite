// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QTime>
#include <QtTest/QTest>
#include <QtWidgets/QLabel>

#include "dialogs/MomentFormatPreview.h"

class TestMomentFormatPreview : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // MomentFormatter honors QLocale for MMMM, Do, etc.; pin EN-US so
        // tests are deterministic regardless of host locale.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void testPreviewUpdatesOnFormatChange()
    {
        Corbomite::MomentFormatPreview w;
        w.setSampleDate(QDateTime(QDate(2026, 4, 15), QTime(14, 30)));
        w.setFormatString(QStringLiteral("YYYY-MM-DD"));

        auto *label = w.findChild<QLabel *>();
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), QStringLiteral("2026-04-15"));
    }

    void testPreviewHandlesInvalidFormat()
    {
        Corbomite::MomentFormatPreview w;
        w.setSampleDate(QDateTime(QDate(2026, 4, 15), QTime(14, 30)));
        w.setFormatString(QStringLiteral("ZZ"));

        auto *label = w.findChild<QLabel *>();
        QVERIFY(label != nullptr);
        // Unknown tokens pass through verbatim per MomentFormatter spec.
        QCOMPARE(label->text(), QStringLiteral("ZZ"));
    }

    void testPreviewWithSampleDateOverride()
    {
        Corbomite::MomentFormatPreview w;
        w.setSampleDate(QDateTime(QDate(1999, 12, 31), QTime(23, 59)));
        w.setFormatString(QStringLiteral("YYYY MMMM Do"));

        auto *label = w.findChild<QLabel *>();
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), QStringLiteral("1999 December 31st"));
    }
};

QTEST_MAIN(TestMomentFormatPreview)
#include "tst_momentformatpreview.moc"
