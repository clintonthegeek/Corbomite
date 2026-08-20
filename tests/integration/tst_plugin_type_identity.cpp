// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster P Phase P4.T1/T2 — the correctness gate this cluster exists to
// prove. Loads the REAL corbomite-file-explorer.so via the exact
// KPluginFactory dlopen path PluginManager uses in production (no
// setFactoryOverride bypass — that would construct the plugin class
// directly in-process and prove nothing about the DSO boundary), lets it
// construct a Corbomite::NotesTreeModel *inside its own compiled code*
// (FileExplorerView does this internally today, independent of this test),
// and qobject_casts the resulting QAbstractItemModel* back to
// Corbomite::NotesTreeModel* from the HOST process.
//
// Per C2 (docs/audit-2026-08-20-shared-libraries-refactor.md's corrections)
// Corbomite::Models is a *confirmed*-diverged type, not a hypothetical one:
// P0.T3 found the exe and two plugin .so's already held two different
// NotesTreeModel::staticMetaObject definitions before Phase P3. Qt6's
// QMetaObject::cast() is a pointer-identity check on the QMetaObject chain
// (`m == this`, not a string comparison) — so two separately-linked private
// copies of the same class's staticMetaObject silently fail this cast,
// with no error, no crash, just a nullptr the caller has to know to check.
// That is the bug Cluster Q already fixed once for libs/vault (Corbomite::
// Plugin itself) and this cluster fixes for the rest of the Corbomite
// libraries plugins can link.
//
// Per D4 ("a gate never seen red proves nothing"), this test must be shown
// failing against pre-P3 code before being trusted green today. Since
// master is already post-P3, that was done by checking out the pre-P3
// commit (before 96f23391/etc) in a scratch worktree, copying this file in,
// and confirming it failed with exactly the nullptr this comment describes
// — see the P4 resolution note in the cluster plan for specifics.
#include <QtTest/QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeView>

#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestPluginTypeIdentity : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void notesTreeModelSurvivesDlopenBoundary();
};

void TestPluginTypeIdentity::notesTreeModelSurvivesDlopenBoundary()
{
    FileSystemAdapter fs;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Vault vault(&fs);
    vault.load(dir.path());
    LinkResolver resolver;
    MetadataCache cache(resolver);
    FileManager fm(&vault, &cache);

    PluginManager pm;
#ifdef CORBOMITE_PLUGIN_DEV_DIR
    pm.setSystemSearchPath(QStringLiteral(CORBOMITE_PLUGIN_DEV_DIR));
#else
    QSKIP("CORBOMITE_PLUGIN_DEV_DIR not defined — can't locate the built plugin tree");
#endif
    pm.setContextConfigurator([&](PluginContext *ctx) {
        ctx->setCoreServices(&vault, &fm, &cache, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);
    });
    pm.discoverPlugins();

    const QString id = QStringLiteral("corbomite-file-explorer");
    QVERIFY2(pm.pluginById(id) != nullptr,
        "corbomite-file-explorer.so not found under CORBOMITE_PLUGIN_DEV_DIR "
        "— build it first");
    QVERIFY(pm.enablePlugin(id));

    const auto *info = pm.pluginById(id);
    QVERIFY(info && info->instance);

    QObject *view = info->instance->createView(nullptr);
    QVERIFY(view);

    auto *tree = view->findChild<QTreeView *>();
    QVERIFY(tree);
    QAbstractItemModel *model = tree->model();
    QVERIFY(model);

    // The actual gate: cross-DSO qobject_cast for a type C2 confirmed as
    // already-diverged pre-P3 (Corbomite::Models). `model` was constructed
    // by code compiled into the dlopen'd plugin .so; this cast runs in the
    // host test process using the host's own resolved copy of
    // NotesTreeModel::staticMetaObject.
    auto *typed = qobject_cast<NotesTreeModel *>(model);
    QVERIFY2(typed != nullptr,
        "qobject_cast<NotesTreeModel*> failed across the host/plugin "
        "boundary — host and the dlopen'd corbomite-file-explorer.so "
        "disagree on NotesTreeModel's QMetaObject identity");

    delete view;
}

QTEST_MAIN(TestPluginTypeIdentity)
#include "tst_plugin_type_identity.moc"
