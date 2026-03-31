// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>
#include <QIcon>
#include "MainWindow.h"
#include "VaultService.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("corbomite");

#ifdef CORBOMITE_DEV_BUILD
    const auto componentName = QStringLiteral("corbomite-dev");
    const auto displayName = i18n("Corbomite [Dev]");
    const auto desktopFile = QStringLiteral("org.corbomite.Corbomite.Dev");
#else
    const auto componentName = QStringLiteral("corbomite");
    const auto displayName = i18n("Corbomite");
    const auto desktopFile = QStringLiteral("org.corbomite.Corbomite");
#endif

    KAboutData aboutData(componentName, displayName, QStringLiteral("0.1.0"),
        i18n("A native Obsidian-inspired knowledge management application"),
        KAboutLicense::GPL_V3, i18n("(c) 2026 Corbomite Contributors"));
    aboutData.setOrganizationDomain("corbomite.org");
    aboutData.setDesktopFileName(desktopFile);
    KAboutData::setApplicationData(aboutData);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));

    KDBusService service(KDBusService::Unique);

    Corbomite::VaultService vaultService;
    Corbomite::MainWindow mainWindow(&vaultService);
    mainWindow.show();

    return app.exec();
}
