// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>
#include <QIcon>
#include "MainWindow.h"
#include "CorbomiteApp.h"
#include "corbomite/core/ScopeManager.h"

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

    // Install the app-wide key-binding dispatcher (Cluster C).
    // Any Modal/Menu that owns a Scope will push/pop onto this.
    Corbomite::ScopeManager::instance()->installOnApplication();

    // Command-line arguments: a positional argument opens the given vault
    // path immediately. Enables `./Corbomite <vault-path>` for dev/testing
    // and one-click launch via file manager.
    QCommandLineParser parser;
    parser.setApplicationDescription(aboutData.shortDescription());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("vault"),
        i18n("Vault directory to open on startup."), QStringLiteral("[vault]"));
    parser.process(app);

    Corbomite::CorbomiteApp corbomiteApp;
    auto *mainWindow = new Corbomite::MainWindow(&corbomiteApp);
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    mainWindow->show();

    const QStringList posArgs = parser.positionalArguments();
    if (!posArgs.isEmpty()) {
        const QString vaultPath = QFileInfo(posArgs.first()).absoluteFilePath();
        if (QDir(vaultPath).exists()) corbomiteApp.openVault(vaultPath);
    }

    return app.exec();
}
