// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/TFile.h"

class TestTFile : public QObject
{
    Q_OBJECT
private slots:
    void basenameAndExtensionDerivedFromCtor();
    void setPathUpdatesBasenameAndExtension();
    void getShortNameMarkdown();
    void getShortNameNonMarkdown();
    void statNullUntilSet();
    void savingDefaultsFalse();
    void extensionIsLowercase();
    void noExtensionHandled();
};

void TestTFile::basenameAndExtensionDerivedFromCtor()
{
    Corbomite::TFile f(nullptr, QStringLiteral("folder/Note.md"));
    QCOMPARE(f.basename, QStringLiteral("Note"));
    QCOMPARE(f.extension, QStringLiteral("md"));
}

void TestTFile::setPathUpdatesBasenameAndExtension()
{
    Corbomite::TFile f(nullptr, QStringLiteral("a.md"));
    f.setPath(QStringLiteral("folder/b.canvas"));
    QCOMPARE(f.basename, QStringLiteral("b"));
    QCOMPARE(f.extension, QStringLiteral("canvas"));
}

void TestTFile::getShortNameMarkdown()
{
    Corbomite::TFile f(nullptr, QStringLiteral("Note.md"));
    QCOMPARE(f.getShortName(), QStringLiteral("Note"));
}

void TestTFile::getShortNameNonMarkdown()
{
    Corbomite::TFile f(nullptr, QStringLiteral("image.png"));
    QCOMPARE(f.getShortName(), QStringLiteral("image.png"));
}

void TestTFile::statNullUntilSet()
{
    Corbomite::TFile f(nullptr, QStringLiteral("a.md"));
    QVERIFY(!f.stat.has_value());
}

void TestTFile::savingDefaultsFalse()
{
    Corbomite::TFile f(nullptr, QStringLiteral("a.md"));
    QCOMPARE(f.saving, false);
}

void TestTFile::extensionIsLowercase()
{
    Corbomite::TFile f(nullptr, QStringLiteral("Note.MD"));
    QCOMPARE(f.extension, QStringLiteral("md"));
}

void TestTFile::noExtensionHandled()
{
    Corbomite::TFile f(nullptr, QStringLiteral("README"));
    QCOMPARE(f.basename, QStringLiteral("README"));
    QCOMPARE(f.extension, QString());
}

QTEST_MAIN(TestTFile)
#include "tst_tfile.moc"
