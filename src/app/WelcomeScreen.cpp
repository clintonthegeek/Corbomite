// SPDX-License-Identifier: GPL-3.0-or-later
#include "WelcomeScreen.h"
#include "VaultService.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPainter>
#include <QPalette>
#include <QRandomGenerator>
#include <QFont>
#include <QFileInfo>
#include <QDir>

namespace Corbomite {

WelcomeScreen::WelcomeScreen(VaultService *vaultService, QWidget *parent)
    : QWidget(parent)
    , m_vaultService(vaultService)
    , m_artworkSeed(QRandomGenerator::global()->bounded(100000))
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setAlignment(Qt::AlignCenter);

    auto *content = new QWidget(this);
    content->setMaximumWidth(500);
    content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setSpacing(16);

    // Artwork placeholder — painted in paintEvent
    auto *artworkWidget = new QWidget(content);
    artworkWidget->setFixedHeight(200);
    artworkWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    contentLayout->addWidget(artworkWidget);

    // Title
    auto *titleLabel = new QLabel(content);
#ifdef CORBOMITE_DEV_BUILD
    titleLabel->setText(QStringLiteral("Corbomite [Dev]"));
#else
    titleLabel->setText(QStringLiteral("Corbomite"));
#endif
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setWeight(QFont::Light);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(titleLabel);

    contentLayout->addSpacing(8);

    // Recent vaults label
    auto *recentLabel = new QLabel(i18n("Recent Vaults"), content);
    QFont recentFont = recentLabel->font();
    recentFont.setWeight(QFont::DemiBold);
    recentLabel->setFont(recentFont);
    contentLayout->addWidget(recentLabel);

    // Recent vaults list
    m_recentList = new QListWidget(content);
    m_recentList->setMaximumHeight(240);
    m_recentList->setAlternatingRowColors(true);
    m_recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    contentLayout->addWidget(m_recentList);

    connect(m_recentList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            Q_EMIT vaultRequested(path);
        }
    });

    connect(m_recentList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            Q_EMIT vaultRequested(path);
        }
    });

    contentLayout->addSpacing(8);

    // Buttons row
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    m_openButton = new QPushButton(i18n("Open Folder..."), content);
    m_openButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    connect(m_openButton, &QPushButton::clicked, this, &WelcomeScreen::openFolderRequested);
    buttonLayout->addWidget(m_openButton);

    m_createButton = new QPushButton(i18n("Create New Vault..."), content);
    m_createButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    connect(m_createButton, &QPushButton::clicked, this, &WelcomeScreen::createVaultRequested);
    buttonLayout->addWidget(m_createButton);

    contentLayout->addLayout(buttonLayout);

    outerLayout->addWidget(content);

    refreshRecentVaults();
}

void WelcomeScreen::refreshRecentVaults()
{
    m_recentList->clear();

    QStringList recent = m_vaultService->recentVaults();
    int count = qMin(recent.size(), 8);

    for (int i = 0; i < count; ++i) {
        const QString &path = recent.at(i);
        QFileInfo info(path);
        QString name = info.fileName();
        QString dir = info.absolutePath();

        auto *item = new QListWidgetItem(m_recentList);
        item->setText(name + QStringLiteral("    ") + dir);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }

    m_recentList->setVisible(count > 0);
}

void WelcomeScreen::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (m_artwork.isNull() || m_artwork.size() != size()) {
        generateArtwork();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_artwork);
}

void WelcomeScreen::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_artwork = QPixmap(); // invalidate
}

void WelcomeScreen::generateArtwork()
{
    if (width() <= 0 || height() <= 0) return;

    m_artwork = QPixmap(size());
    m_artwork.fill(Qt::transparent);

    QPainter painter(&m_artwork);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRandomGenerator rng(m_artworkSeed);
    const QPalette &pal = palette();

    QColor highlight = pal.color(QPalette::Highlight);
    QColor accent = pal.color(QPalette::Link);
    QColor base = pal.color(QPalette::Base);

    QList<QColor> colors = {
        highlight,
        accent,
        QColor(highlight.red(), accent.green(), base.blue()),
        QColor(accent.red(), highlight.green(), accent.blue()),
    };

    int artHeight = qMin(200, height() / 3);
    int centerX = width() / 2;
    int centerY = artHeight / 2;

    for (int i = 0; i < 7; ++i) {
        QColor c = colors.at(rng.bounded(colors.size()));
        c.setAlpha(30 + rng.bounded(40));

        int rx = 60 + rng.bounded(140);
        int ry = 40 + rng.bounded(80);
        int ox = centerX + rng.bounded(201) - 100;
        int oy = centerY + rng.bounded(101) - 50;

        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
        painter.drawEllipse(QPoint(ox, oy), rx, ry);
    }
}

} // namespace Corbomite
