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
                               KActionCollection *actions,
                               QWidget *parent)
    : KPageDialog(parent),
      m_plugins(plugins),
      m_themeService(themeService),
      m_actions(actions)
{
    setWindowTitle(i18n("Settings"));
    setFaceType(KPageDialog::List);
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);

    setupEditorPage();
    setupFilesPage();
    setupAppearancePage();
    setupDailyNotesPage();
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
        connect(editorTheme, &QComboBox::currentTextChanged,
                this, [this](const QString &name) {
            if (m_themeService) m_themeService->setActiveThemeByName(name);
            CorbomiteSettings::self()->setMarkoffTheme(name);
            CorbomiteSettings::self()->save();
        });
        layout->addRow(i18n("Editor theme:"), editorTheme);

        auto *importBtn = new QPushButton(i18n("Import QOwnNotes scheme…"));
        connect(importBtn, &QPushButton::clicked, this, [this, editorTheme] {
            const QString home = QDir::homePath();
            const QString suggestedDir = home + QStringLiteral("/.config/PBE/QOwnNotes");
            const QString path = QFileDialog::getOpenFileName(
                this, i18n("Import QOwnNotes scheme"),
                QDir(suggestedDir).exists() ? suggestedDir : home,
                i18n("INI files (*.conf *.ini);;All files (*)"));
            if (path.isEmpty()) return;

            const auto theme = Markoff::Theme::importFromQOwnNotesIni(path);
            if (!theme) {
                KMessageBox::error(this,
                    i18n("Could not parse %1 as a QOwnNotes scheme file.", path),
                    i18n("Import failed"));
                return;
            }
            if (!m_themeService) return;
            m_themeService->addUserTheme(*theme);
            // Refresh combobox preserving signal-blocking so we don't
            // apply twice.
            editorTheme->blockSignals(true);
            editorTheme->clear();
            editorTheme->addItems(m_themeService->availableThemeNames());
            editorTheme->setCurrentText(theme->name);
            editorTheme->blockSignals(false);
            m_themeService->setActiveThemeByName(theme->name);
            CorbomiteSettings::self()->setMarkoffTheme(theme->name);
            CorbomiteSettings::self()->save();
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

    if (m_actions) {
        // KShortcutsEditor embeds inline; the standalone KShortcutsDialog
        // wraps the same widget but doesn't fit KPageDialog's
        // single-instance navigation. Reset target stays on Default;
        // changes commit on apply via the editor's own save/undo.
        auto *editor = new KShortcutsEditor(
            m_actions,
            page,
            KShortcutsEditor::AllActions,
            KShortcutsEditor::LetterShortcutsAllowed);
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
    if (auto *w = findChild<QSpinBox *>(QStringLiteral("autoSaveDelay")))
        settings->setAutoSaveDelayMs(w->value());
    if (auto *w = findChild<QComboBox *>(QStringLiteral("trashOption")))
        settings->setTrashOption(w->currentData().toString());
    if (auto *w = findChild<QCheckBox *>(QStringLiteral("promptDelete")))
        settings->setPromptDelete(w->isChecked());
    if (auto *w = findChild<QComboBox *>(QStringLiteral("theme")))
        settings->setTheme(w->currentData().toString());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("templateFolder")))
        settings->setTemplateFolder(w->text());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("dailyNoteFolder")))
        settings->setDailyNoteFolder(w->text());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("dailyNoteDateFormat")))
        settings->setDailyNoteDateFormat(w->text());
    if (auto *w = findChild<QLineEdit *>(QStringLiteral("dailyNoteTemplate")))
        settings->setDailyNoteTemplate(w->text());

    settings->save();
}

} // namespace Corbomite
