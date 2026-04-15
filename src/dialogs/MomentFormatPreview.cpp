// SPDX-License-Identifier: GPL-3.0-or-later
#include "MomentFormatPreview.h"

#include "corbomite/core/MomentFormatter.h"

#include <QtCore/QTimer>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

namespace Corbomite {

MomentFormatPreview::MomentFormatPreview(QWidget *parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
    , m_tickTimer(new QTimer(this))
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // Live update every second so seconds-valued formats tick.
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &MomentFormatPreview::onTick);
    m_tickTimer->start();

    refresh();
}

MomentFormatPreview::~MomentFormatPreview() = default;

void MomentFormatPreview::setFormatString(const QString &format)
{
    if (m_format == format) return;
    m_format = format;
    refresh();
}

void MomentFormatPreview::setSampleDate(const QDateTime &sample)
{
    m_sampleDate = sample;
    if (sample.isValid()) {
        m_tickTimer->stop();
    } else {
        m_tickTimer->start();
    }
    refresh();
}

void MomentFormatPreview::clearSampleDate()
{
    m_sampleDate = QDateTime{};
    m_tickTimer->start();
    refresh();
}

QString MomentFormatPreview::formatString() const { return m_format; }

void MomentFormatPreview::onTick() { refresh(); }

void MomentFormatPreview::refresh()
{
    const QDateTime when = m_sampleDate.isValid()
        ? m_sampleDate
        : QDateTime::currentDateTime();
    if (m_format.isEmpty()) {
        m_label->setText(QStringLiteral("\u2014"));
    } else {
        m_label->setText(Corbomite::MomentFormatter::format(when, m_format));
    }
}

}  // namespace Corbomite
