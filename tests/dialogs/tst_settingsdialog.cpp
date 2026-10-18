// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings → Appearance's editor-theme dropdown had no writer. This dialog is
// a KPageDialog whose `applySettings()` enumerates its widgets by objectName —
// it is not a KConfigDialog, so no standard binding exists. The
// `markoffTheme` branch was missing from that enumeration, so every Apply/OK
// saved the other keys and silently left `MarkoffTheme` at its "Follow system"
// default: the dropdown moved, the editor never did. Reported by the user on
// 2026-10-18 (app themes worked; editor themes were ignored).
//
// KPageWidget mounts a page's widgets only once that page has been shown, so
// these cases visit Appearance first — which is also what a person must do to
// change the value. Verified live 2026-10-18: with Appearance visited, Apply
// writes `MarkoffTheme`; from the Editor page alone, findChild() legitimately
// does not see it (an unvisited page's values are still the ones loaded from
// kcfg, so nothing is lost by that).

#include <QTest>

#include <QComboBox>
#include <QListView>
#include <QPushButton>
#include <QStandardPaths>

#include <KConfigGroup>
#include <KSharedConfig>

#include "corbomite/core/ThemeService.h"
#include "corbomitesettings.h"
#include "dialogs/SettingsDialog.h"

using namespace Corbomite;

namespace {

/// Select a page by title, the way clicking the page list does. Returns false
/// when no page carries that title.
bool visitPage(SettingsDialog &dialog, const QString &title)
{
    auto *view = dialog.findChild<QListView *>();
    if (!view || !view->model())
        return false;
    auto *model = view->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data().toString() == title) {
            view->setCurrentIndex(index);
            return true;
        }
    }
    return false;
}

} // namespace

class TestSettingsDialog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void editorThemeComboListsTheServiceNames();
    void applyButtonPersistsTheChosenEditorTheme();
    void okPersistsTheChosenEditorTheme();
};

void TestSettingsDialog::initTestCase()
{
    // CorbomiteSettings is a singleton over KSharedConfig::openConfig(), whose
    // default file name is the application name — so this binary already owns
    // its own rc file. Test mode keeps that file under the test's temp config
    // directory instead of the developer's ~/.config.
    QStandardPaths::setTestModeEnabled(true);
}

void TestSettingsDialog::editorThemeComboListsTheServiceNames()
{
    Core::ThemeService service(nullptr);
    SettingsDialog dialog(nullptr, &service);
    dialog.show();

    QVERIFY(visitPage(dialog, QStringLiteral("Appearance")));
    QCoreApplication::processEvents();

    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("markoffTheme"));
    QVERIFY2(combo, "the Appearance page must expose the editor-theme combo");
    QVERIFY(combo->findText(QStringLiteral("Follow system")) >= 0);
    QVERIFY(combo->findText(QStringLiteral("Light")) >= 0);
    QVERIFY(combo->findText(QStringLiteral("Dark")) >= 0);
}

void TestSettingsDialog::applyButtonPersistsTheChosenEditorTheme()
{
    Core::ThemeService service(nullptr);
    SettingsDialog dialog(nullptr, &service);
    dialog.show();

    QVERIFY(visitPage(dialog, QStringLiteral("Appearance")));
    QCoreApplication::processEvents();

    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("markoffTheme"));
    QVERIFY(combo);

    // Independent of any earlier case: the singleton persists across cases.
    CorbomiteSettings::self()->setMarkoffTheme(QStringLiteral("Follow system"));

    // Falsifiable: without the `markoffTheme` branch in applySettings() the
    // key stays at the sentinel, which is exactly the reported symptom.
    QCOMPARE(CorbomiteSettings::self()->markoffTheme(), QStringLiteral("Follow system"));

    combo->setCurrentText(QStringLiteral("Dark"));

    auto *apply = dialog.button(QDialogButtonBox::Apply);
    QVERIFY2(apply, "the dialog must carry an Apply button");
    apply->click();

    QCOMPARE(CorbomiteSettings::self()->markoffTheme(), QStringLiteral("Dark"));
    const KConfigGroup appearance(KSharedConfig::openConfig(),
                                  QStringLiteral("Appearance"));
    QCOMPARE(appearance.readEntry("MarkoffTheme"), QStringLiteral("Dark"));
}

void TestSettingsDialog::okPersistsTheChosenEditorTheme()
{
    Core::ThemeService service(nullptr);
    SettingsDialog dialog(nullptr, &service);
    dialog.show();

    QVERIFY(visitPage(dialog, QStringLiteral("Appearance")));
    QCoreApplication::processEvents();

    auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("markoffTheme"));
    QVERIFY(combo);
    CorbomiteSettings::self()->setMarkoffTheme(QStringLiteral("Follow system"));
    combo->setCurrentText(QStringLiteral("Light"));

    dialog.accept();  // OK
    QCOMPARE(CorbomiteSettings::self()->markoffTheme(), QStringLiteral("Light"));
}

QTEST_MAIN(TestSettingsDialog)
#include "tst_settingsdialog.moc"
