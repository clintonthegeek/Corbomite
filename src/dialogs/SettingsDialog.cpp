// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"
#include "corbomitesettings.h"
#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

namespace Corbomite {

SettingsDialog::SettingsDialog(QWidget *parent)
    : KPageDialog(parent)
{
    setWindowTitle(i18n("Settings"));
    setFaceType(KPageDialog::List);
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);

    setupEditorPage();
    setupFilesPage();
    setupAppearancePage();

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

    auto item = addPage(page, i18n("Appearance"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme")));
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

    settings->save();
}

} // namespace Corbomite
