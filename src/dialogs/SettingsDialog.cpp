// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"
#include "MomentFormatPreview.h"
#include "PluginsPage.h"
#include "corbomite/core/ThemeService.h"
#include "corbomitesettings.h"
#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KShortcutsEditor>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace Corbomite {

SettingsDialog::SettingsDialog(PluginManager *plugins,
                               Core::ThemeService *themeService,
                               const ActionCollections &actionCollections,
                               QWidget *parent)
    : KPageDialog(parent),
      m_plugins(plugins),
      m_themeService(themeService),
      m_actionCollections(actionCollections)
{
    setWindowTitle(i18n("Settings"));
    setFaceType(KPageDialog::List);
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);

    setupEditorPage();
    setupFilesPage();
    setupAppearancePage();
    setupDailyNotesPage();
    setupFileRecoveryPage();
    setupHotkeysPage();
    setupPluginsPage();

    connect(this, &QDialog::accepted, this, &SettingsDialog::applySettings);
    connect(button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::applySettings);
}

void SettingsDialog::setupEditorPage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    auto *settings = CorbomiteSettings::self();

    auto *fontSize = new QSpinBox;
    fontSize->setRange(6, 72);
    fontSize->setValue(settings->fontSize());
    fontSize->setObjectName(QStringLiteral("fontSize"));
    layout->addRow(i18n("Font size:"), fontSize);

    auto *tabSize = new QSpinBox;
    tabSize->setRange(1, 8);
    tabSize->setValue(settings->tabSize());
    tabSize->setObjectName(QStringLiteral("tabSize"));
    layout->addRow(i18n("Tab size:"), tabSize);

    auto *lineNumbers = new QCheckBox;
    lineNumbers->setChecked(settings->lineNumbers());
    lineNumbers->setObjectName(QStringLiteral("lineNumbers"));
    layout->addRow(i18n("Show line numbers:"), lineNumbers);

    auto *lineWrap = new QCheckBox;
    lineWrap->setChecked(settings->lineWrap());
    lineWrap->setObjectName(QStringLiteral("lineWrap"));
    layout->addRow(i18n("Wrap long lines:"), lineWrap);

    // Readable line width implies word-wrap (a fixed-width column with
    // wrap off would just clip text at the column edge) — reflect that
    // dependency in the UI rather than silently overriding lineWrap's
    // saved value at apply time.
    auto *readableLineWidth = new QCheckBox;
    readableLineWidth->setChecked(settings->readableLineWidth());
    readableLineWidth->setObjectName(QStringLiteral("readableLineWidth"));
    readableLineWidth->setToolTip(i18n("Centers a fixed-width readable column, matching Obsidian's default."));
    layout->addRow(i18n("Readable line width:"), readableLineWidth);

    auto syncLineWrapToReadableWidth = [lineWrap](bool readable) {
        lineWrap->setChecked(lineWrap->isChecked() || readable);
        lineWrap->setDisabled(readable);
    };
    syncLineWrapToReadableWidth(readableLineWidth->isChecked());
    connect(readableLineWidth, &QCheckBox::toggled, lineWrap, syncLineWrapToReadableWidth);

    auto *autoSave = new QSpinBox;
    autoSave->setRange(500, 30000);
    autoSave->setSingleStep(500);
    autoSave->setSuffix(i18n(" ms"));
    autoSave->setValue(settings->autoSaveDelayMs());
    autoSave->setObjectName(QStringLiteral("autoSaveDelay"));
    layout->addRow(i18n("Autosave delay:"), autoSave);

    auto item = addPage(page, i18n("Editor"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));
}

void SettingsDialog::setupFilesPage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    auto *settings = CorbomiteSettings::self();

    auto *trashOption = new QComboBox;
    trashOption->addItem(i18n("System Trash"), QStringLiteral("system"));
    trashOption->addItem(i18n("Vault Trash (.trash)"), QStringLiteral("vault"));
    trashOption->addItem(i18n("Permanently Delete"), QStringLiteral("permanent"));
    int idx = trashOption->findData(settings->trashOption());
    if (idx >= 0) trashOption->setCurrentIndex(idx);
    trashOption->setObjectName(QStringLiteral("trashOption"));
    layout->addRow(i18n("Delete behavior:"), trashOption);

    auto *promptDelete = new QCheckBox;
    promptDelete->setChecked(settings->promptDelete());
    promptDelete->setObjectName(QStringLiteral("promptDelete"));
    layout->addRow(i18n("Confirm before delete:"), promptDelete);

    auto item = addPage(page, i18n("Files"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("folder")));
}

void SettingsDialog::setupAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    auto *settings = CorbomiteSettings::self();

    auto *theme = new QComboBox;
    theme->addItem(i18n("System"), QStringLiteral("system"));
    theme->addItem(i18n("Light"), QStringLiteral("light"));
    theme->addItem(i18n("Dark"), QStringLiteral("dark"));
    int idx = theme->findData(settings->theme());
    if (idx >= 0) theme->setCurrentIndex(idx);
    theme->setObjectName(QStringLiteral("theme"));
    layout->addRow(i18n("Theme:"), theme);

    // C2 — Editor (Markoff) theme combobox + QOwnNotes import button.
    if (m_themeService) {
        auto *editorTheme = new QComboBox;
        const auto names = m_themeService->availableThemeNames();
        editorTheme->addItems(names);
        editorTheme->setCurrentText(m_themeService->activeThemeName());
        editorTheme->setObjectName(QStringLiteral("markoffTheme"));
        // NO currentTextChanged handler. There was one, and it was wrong on
        // two counts the user reported:
        //
        //  1. It applied IMMEDIATELY and called save(), so moving the dropdown
        //     repainted the editor and wrote to disk before OK/Apply — and
        //     left Cancel unable to revert anything. This is a KPageDialog
        //     with Ok/Cancel/Apply; a selection is a *proposal* until Apply.
        //  2. Because it drove the live ThemeService on selection, the
        //     dialog and the running editor could disagree: the kcfg key was
        //     written at selection time while the service's active name was
        //     only re-read from kcfg at MainWindow construction.
        //
        // So the selection is only read on OK/Apply, by applySettings() below
        // — this dialog is a KPageDialog, NOT a KConfigDialog, so there is no
        // standard binding to rely on. `applySettings()` must list this widget
        // or the choice is silently dropped (it was, until 2026-10-18).
        layout->addRow(i18n("Editor theme:"), editorTheme);

        auto *importBtn = new QPushButton(i18n("Import QOwnNotes scheme…"));
        connect(importBtn, &QPushButton::clicked, this, [this, editorTheme] {
            // Importing a QOwnNotes .ini scheme needs Markoff's theme
            // importer, which does not exist yet — the old one went with the
            // previous theme shape. Say so before asking the user to pick a
            // file, rather than after.
            KMessageBox::information(this,
                i18n("Importing a QOwnNotes scheme is not implemented yet."),
                i18n("Import unavailable"));
            return;
        });
        layout->addRow(QString(), importBtn);
    }

    auto item = addPage(page, i18n("Appearance"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")));
}

void SettingsDialog::setupDailyNotesPage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    auto *settings = CorbomiteSettings::self();

    auto *templateFolder = new QLineEdit;
    templateFolder->setText(settings->templateFolder());
    templateFolder->setObjectName(QStringLiteral("templateFolder"));
    layout->addRow(i18n("Template folder:"), templateFolder);

    auto *dailyFolder = new QLineEdit;
    dailyFolder->setText(settings->dailyNoteFolder());
    dailyFolder->setObjectName(QStringLiteral("dailyNoteFolder"));
    layout->addRow(i18n("Daily notes folder:"), dailyFolder);

    auto *dailyFormat = new QLineEdit;
    dailyFormat->setText(settings->dailyNoteDateFormat());
    dailyFormat->setObjectName(QStringLiteral("dailyNoteDateFormat"));
    layout->addRow(i18n("Daily note date format:"), dailyFormat);

    auto *dailyFormatPreview = new MomentFormatPreview(page);
    dailyFormatPreview->setObjectName(QStringLiteral("dailyNoteDateFormatPreview"));
    dailyFormatPreview->setFormatString(dailyFormat->text());
    connect(dailyFormat, &QLineEdit::textChanged,
            dailyFormatPreview, &MomentFormatPreview::setFormatString);
    layout->addRow(i18n("Preview:"), dailyFormatPreview);

    auto *dailyTemplate = new QLineEdit;
    dailyTemplate->setText(settings->dailyNoteTemplate());
    dailyTemplate->setObjectName(QStringLiteral("dailyNoteTemplate"));
    layout->addRow(i18n("Daily note template:"), dailyTemplate);

    auto item = addPage(page, i18n("Daily Notes"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar-day")));
}

void SettingsDialog::setupFileRecoveryPage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    auto *settings = CorbomiteSettings::self();

    // Snapshot interval: Obsidian's dropdown is Off / 5 / 15 / 30 / 60 / 120.
    auto *interval = new QComboBox;
    interval->addItem(i18n("Off"), 0);
    interval->addItem(i18n("5 minutes"), 5);
    interval->addItem(i18n("15 minutes"), 15);
    interval->addItem(i18n("30 minutes"), 30);
    interval->addItem(i18n("60 minutes"), 60);
    interval->addItem(i18n("120 minutes"), 120);
    int idx = interval->findData(settings->intervalMinutes());
    if (idx >= 0) interval->setCurrentIndex(idx);
    interval->setObjectName(QStringLiteral("recoveryIntervalMinutes"));
    layout->addRow(i18n("Snapshot interval:"), interval);

    auto *maxAge = new QComboBox;
    for (int days : {1, 3, 7, 14, 30})
        maxAge->addItem(i18np("%1 day", "%1 days", days), days);
    int ageIdx = maxAge->findData(settings->maxAgeDays());
    if (ageIdx >= 0) maxAge->setCurrentIndex(ageIdx);
    maxAge->setObjectName(QStringLiteral("recoveryMaxAgeDays"));
    layout->addRow(i18n("Maximum snapshot age:"), maxAge);

    auto *maxSnaps = new QSpinBox;
    maxSnaps->setRange(1, 1000);
    maxSnaps->setValue(settings->maxSnapshotsPerFile());
    maxSnaps->setObjectName(QStringLiteral("recoveryMaxSnapshotsPerFile"));
    layout->addRow(i18n("Maximum snapshots per file:"), maxSnaps);

    auto *note = new QLabel(i18n(
        "Snapshots are stored outside the vault and are pruned by age and "
        "count. Save-failure backups are never pruned."));
    note->setWordWrap(true);
    layout->addRow(note);

    auto item = addPage(page, i18n("File Recovery"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("document-revert")));
}

void SettingsDialog::setupPluginsPage()
{
    auto *page = new PluginsPage(m_plugins);
    auto *item = addPage(page, i18n("Plugins"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-plugin")));
}

void SettingsDialog::setupHotkeysPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    if (!m_actionCollections.isEmpty()) {
        // KShortcutsEditor embeds inline; the standalone KShortcutsDialog
        // wraps the same widget but doesn't fit KPageDialog's
        // single-instance navigation. Reset target stays on Default;
        // changes commit on apply via the editor's own save/undo.
        //
        // One collection is no longer enough:
        // every ViewActions provider's collection must show here too
        // (eagerly constructed, so its shortcuts exist even with no tab
        // of that type open), or the Hotkeys page would only ever show
        // whichever type happened to be focused when Settings was opened.
        // Built via the no-arg-collection overload + addCollection() per
        // entry rather than passing the first collection through the
        // constructor, so every entry (including the first) gets an
        // explicit section title.
        auto *editor = new KShortcutsEditor(
            page,
            KShortcutsEditor::AllActions,
            KShortcutsEditor::LetterShortcutsAllowed);
        for (const auto &[collection, title] : m_actionCollections) {
            if (collection) editor->addCollection(collection, title);
        }
        layout->addWidget(editor);
        // KShortcutsEditor edits in-memory; commit on dialog accept,
        // discard on cancel (the matching XMLGUI rc-file save happens at
        // host level when actions persist).
        connect(this, &QDialog::accepted, editor, &KShortcutsEditor::save);
        connect(this, &QDialog::rejected, editor, &KShortcutsEditor::undo);
    } else {
        auto *empty = new QLabel(
            i18n("No action collection available — open a vault to "
                 "configure shortcuts."),
            page);
        empty->setWordWrap(true);
        layout->addWidget(empty);
        layout->addStretch();
    }

    auto *item = addPage(page, i18n("Hotkeys"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
}

void SettingsDialog::applySettings()
{
    auto *settings = CorbomiteSettings::self();

    if (auto *w = findChild<QSpinBox *>(QStringLiteral("fontSize")))
        settings->setFontSize(w->value());
    if (auto *w = findChild<QSpinBox *>(QStringLiteral("tabSize")))
        settings->setTabSize(w->value());
    if (auto *w = findChild<QCheckBox *>(QStringLiteral("lineNumbers")))
        settings->setLineNumbers(w->isChecked());
    if (auto *w = findChild<QCheckBox *>(QStringLiteral("lineWrap")))
        settings->setLineWrap(w->isChecked());
    if (auto *w = findChild<QCheckBox *>(QStringLiteral("readableLineWidth")))
        settings->setReadableLineWidth(w->isChecked());
    if (auto *w = findChild<QSpinBox *>(QStringLiteral("autoSaveDelay")))
        settings->setAutoSaveDelayMs(w->value());
    if (auto *w = findChild<QComboBox *>(QStringLiteral("trashOption")))
        settings->setTrashOption(w->currentData().toString());
    if (auto *w = findChild<QCheckBox *>(QStringLiteral("promptDelete")))
        settings->setPromptDelete(w->isChecked());
    if (auto *w = findChild<QComboBox *>(QStringLiteral("theme")))
        settings->setTheme(w->currentData().toString());
    // Editor theme. The combo carries theme *names* as its text (the service's
    // own vocabulary), and there is no other writer of `MarkoffTheme` — this
    // dialog is a KPageDialog with this hand-written applySettings, not a
    // KConfigDialog, so nothing binds the widget to the key automatically.
    // Missing this branch is what left the key at its "Follow system" default
    // for every user: the editor dropdown moved, Apply/OK wrote every other
    // key, and the editor kept tracking the app palette. See
    // `tst_settingsdialog`.
    if (auto *w = findChild<QComboBox *>(QStringLiteral("markoffTheme")))
        settings->setMarkoffTheme(w->currentText());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("templateFolder")))
        settings->setTemplateFolder(w->text());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("dailyNoteFolder")))
        settings->setDailyNoteFolder(w->text());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("dailyNoteDateFormat")))
        settings->setDailyNoteDateFormat(w->text());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("dailyNoteTemplate")))
        settings->setDailyNoteTemplate(w->text());
    if (auto *w = findChild<QComboBox *>(QStringLiteral("recoveryIntervalMinutes")))
        settings->setIntervalMinutes(w->currentData().toInt());
    if (auto *w = findChild<QComboBox *>(QStringLiteral("recoveryMaxAgeDays")))
        settings->setMaxAgeDays(w->currentData().toInt());
    if (auto *w = findChild<QSpinBox *>(QStringLiteral("recoveryMaxSnapshotsPerFile")))
        settings->setMaxSnapshotsPerFile(w->value());

    settings->save();
}

} // namespace Corbomite
