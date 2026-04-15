// SPDX-License-Identifier: GPL-3.0-or-later
#include "Notice.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>

namespace Corbomite {

Notice::Notice(const QString &message, int durationMs, QWidget *parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 90));
    setGraphicsEffect(shadow);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(12, 8, 12, 8);
    m_layout->setSpacing(8);

    m_label = new QLabel(message, this);
    m_label->setWordWrap(true);
    m_label->setMinimumWidth(200);
    m_label->setMaximumWidth(360);
    m_layout->addWidget(m_label);

    m_dismissTimer.setSingleShot(true);
    m_dismissTimer.setInterval(durationMs);
    connect(&m_dismissTimer, &QTimer::timeout, this, &QWidget::close);
}

void Notice::setAction(const QString &label, std::function<void()> callback)
{
    if (m_actionButton) {
        m_layout->removeWidget(m_actionButton);
        m_actionButton->deleteLater();
    }
    m_actionButton = new QPushButton(label, this);
    m_actionButton->setFlat(true);
    m_layout->addWidget(m_actionButton);
    if (callback) {
        connect(m_actionButton, &QPushButton::clicked, this, [this, cb = std::move(callback)]() {
            cb();
            close();
        });
    } else {
        connect(m_actionButton, &QPushButton::clicked, this, &QWidget::close);
    }
}

QString Notice::message() const
{
    return m_label ? m_label->text() : QString();
}

void Notice::anchorBottomRight()
{
    QScreen *screen = QApplication::primaryScreen();
    if (auto *win = window(); win && win->screen()) screen = win->screen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    adjustSize();
    constexpr int margin = 16;
    move(avail.right() - width() - margin, avail.bottom() - height() - margin);
}

void Notice::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    anchorBottomRight();
    raise();
    m_dismissTimer.start();
}

} // namespace Corbomite
