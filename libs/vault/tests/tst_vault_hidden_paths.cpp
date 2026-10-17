// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

/// Obsidian's hidden-path predicate is `Il` (`app/own/0362-Il.js:2-9`): every
/// ancestor segment of the vault-relative path is checked for a leading dot,
/// with no `configDir` parameterisation and no reserved-name list. The audit
/// doc states it backwards (`domains/vault.md` §3: "most dotfiles are hidden
/// *except under* configDir"), which is indistinguishable from the truth for
/// anyone testing with the default config dir name.
class TestVaultHiddenPaths : public QObject
{
    Q_OBJECT
private slots:
    void ordinaryPathsAreVisible();
    void userDotFolderIsHidden();
    void dotSegmentMidPathHides();
    void dotSegmentLeafHides();
    void configDirContentsAreNotExempt();
    void hiddenPathsAreAbsentFromEveryListing();
    void watcherDoesNotEmitCreatedForHiddenPaths();
    void userIgnoreFilterDoesNotHideAnythingFromTheTree();
    void configDirIsStillReadableWhenHidden();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(body);
}

/// Obsidian's adapter lists dot entries and lets `Il` filter them
/// (`listRecursive` at app/own/0376-_l.js:63 calls `reconcileDeletion` for a
/// hidden child, so the *listing* still carries it). `FileSystemAdapter::list`
/// happens to hide them as a side effect of `QDir::AllEntries` without
/// `QDir::Hidden` — so `Vault::buildTree` cannot be allowed to lean on that.
class DotListingAdapter : public Corbomite::FileSystemAdapter
{
public:
    QStringList list(const QString &dirPath) const override
    {
        QDir d(dirPath);
        if (!d.exists()) return {};
        return d.entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                           QDir::NoSort);
    }
};
}  // namespace

void TestVaultHiddenPaths::ordinaryPathsAreVisible()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/note.md", "x");
    writeFile(dir.path() + "/sub/deep/note.md", "y");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QVERIFY(vault.getFileByPath(QStringLiteral("note.md")));
    QVERIFY(vault.getFileByPath(QStringLiteral("sub/deep/note.md")));
}

void TestVaultHiddenPaths::userDotFolderIsHidden()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/note.md", "x");
    writeFile(dir.path() + "/.johns-notes/secret.md", "y");
    writeFile(dir.path() + "/.obs/app.json", "{}");

    DotListingAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    // `.obs` is not `.obsidian`, and `.johns-notes` is not on any reserved
    // list — neither may be special-cased.
    QVERIFY(vault.getFileByPath(QStringLiteral("note.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral(".johns-notes/secret.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral(".obs/app.json")));
    QVERIFY(!vault.getFolderByPath(QStringLiteral(".johns-notes")));
}

void TestVaultHiddenPaths::dotSegmentMidPathHides()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Notes/visible.md", "x");
    writeFile(dir.path() + "/Notes/.hidden/secret.md", "y");

    DotListingAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    // A basename-only predicate would let `.hidden/secret.md` through.
    QVERIFY(vault.getFileByPath(QStringLiteral("Notes/visible.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral("Notes/.hidden/secret.md")));
    QVERIFY(!vault.getFolderByPath(QStringLiteral("Notes/.hidden")));
}

void TestVaultHiddenPaths::dotSegmentLeafHides()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/.hidden.md", "x");
    writeFile(dir.path() + "/ok.md", "y");

    DotListingAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QVERIFY(vault.getFileByPath(QStringLiteral("ok.md")));
    QVERIFY(!vault.getFileByPath(QStringLiteral(".hidden.md")));
}

void TestVaultHiddenPaths::configDirContentsAreNotExempt()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/.obs/app.json", "{}");
    writeFile(dir.path() + "/.obsidian/app.json", "{}");

    DotListingAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    vault.setConfigDir(QStringLiteral(".obs"));
    QCOMPARE(vault.configDir(), QStringLiteral(".obs"));

    // The distinguishing assertion: an "exempt under configDir" reading would
    // make the *configured* config dir's contents visible. Obsidian's `Il`
    // takes no configDir argument, so both stay hidden.
    QVERIFY(!vault.getFileByPath(QStringLiteral(".obs/app.json")));
    QVERIFY(!vault.getFileByPath(QStringLiteral(".obsidian/app.json")));
    QVERIFY(!vault.getFolderByPath(QStringLiteral(".obs")));
}

void TestVaultHiddenPaths::hiddenPathsAreAbsentFromEveryListing()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/note.md", "x");
    writeFile(dir.path() + "/.johns-notes/secret.md", "y");

    DotListingAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QCOMPARE(vault.getFiles().size(), 1);
    QCOMPARE(vault.getMarkdownFiles().size(), 1);
    QCOMPARE(vault.getAllLoadedFiles().size(), 1);
}

void TestVaultHiddenPaths::watcherDoesNotEmitCreatedForHiddenPaths()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/note.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);
    writeFile(dir.path() + "/.johns-notes/late.md", "y");
    writeFile(dir.path() + "/late.md", "z");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<Corbomite::TAbstractFile *>(spy.at(0).at(0))->path,
             QStringLiteral("late.md"));
    QVERIFY(!vault.getFileByPath(QStringLiteral(".johns-notes/late.md")));
}

// Guard, not a red-before test: `Vault` has no ignore-filter surface, so this
// passes both before and after. It exists so that wiring `userIgnoreFilters`
// into the tree — the wrong reading of the setting — fails here. In Obsidian
// the filter gates search results / graph nodes / property-edit candidates and
// nothing else; an ignored note stays in `fileMap`, in `getFiles()`, and in
// link resolution. Only a dot segment hides a path.
void TestVaultHiddenPaths::userIgnoreFilterDoesNotHideAnythingFromTheTree()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/.obsidian/app.json",
              R"({"userIgnoreFilters":["drafts/","/^private/"]})");
    writeFile(dir.path() + "/drafts/wip.md", "x");
    writeFile(dir.path() + "/private/secret.md", "y");
    writeFile(dir.path() + "/kept.md", "z");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    const auto filter = vault.vaultConfig().userIgnoreFilters();
    QVERIFY(!filter.isEmpty());  // the setting really is there to be ignored

    QVERIFY(vault.getFileByPath(QStringLiteral("drafts/wip.md")));
    QVERIFY(vault.getFileByPath(QStringLiteral("private/secret.md")));
    QVERIFY(vault.getFileByPath(QStringLiteral("kept.md")));
    QCOMPARE(vault.getMarkdownFiles().size(), 3);
}

void TestVaultHiddenPaths::configDirIsStillReadableWhenHidden()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/.obsidian/app.json", R"({"alwaysUpdateLinks":true})");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    // Hiding the config dir from the *tree* must not stop VaultConfig from
    // reading it — that path goes through the adapter, not fileMap.
    const auto app = vault.readConfigJson(QStringLiteral("app"));
    QVERIFY(app.isObject());
    QCOMPARE(app.toObject().value(QStringLiteral("alwaysUpdateLinks")).toBool(),
             true);
}

QTEST_MAIN(TestVaultHiddenPaths)
#include "tst_vault_hidden_paths.moc"