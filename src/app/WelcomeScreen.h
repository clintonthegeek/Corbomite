// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QPixmap>

class QListWidget;
class QPushButton;

namespace Corbomite {

class CorbomiteApp;

class WelcomeScreen : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeScreen(CorbomiteApp *app, QWidget *parent = nullptr);

    void refreshRecentVaults();

Q_SIGNALS:
    void vaultRequested(const QString &path);
    void openFolderRequested();
    void createVaultRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void generateArtwork();

    CorbomiteApp *m_app;
    QListWidget *m_recentList;
    QPushButton *m_openButton;
    QPushButton *m_createButton;
    QPixmap m_artwork;
    int m_artworkSeed;
};

} // namespace Corbomite
