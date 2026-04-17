// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileManager : public QObject
{
    Q_OBJECT
private slots:
    void constructsWithVault();
};

void TestFileManager::constructsWithVault()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    Corbomite::FileManager fm(&vault, /*cache=*/nullptr);
    QVERIFY(fm.vault() == &vault);
}

QTEST_MAIN(TestFileManager)
#include "tst_file_manager.moc"
