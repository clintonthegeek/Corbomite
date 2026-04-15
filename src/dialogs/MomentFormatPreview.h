// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

class QLabel;
class QTimer;

namespace Corbomite {

/// Shows a live-updating rendering of a Moment.js format string applied to
/// QDateTime::currentDateTime() (or a fixed sampleDate if provided).
/// Updates when setFormatString() is called. Optionally ticks every second
/// so seconds-valued format strings stay "live" against the user's clock.
class MomentFormatPreview : public QWidget {
    Q_OBJECT
public:
    explicit MomentFormatPreview(QWidget *parent = nullptr);
    ~MomentFormatPreview() override;

    /// Set the format string. Preview re-renders immediately.
    void setFormatString(const QString &format);

    /// Optional: preview against a fixed date instead of "now()". If a
    /// non-null sample is set, the live clock stops ticking.
    void setSampleDate(const QDateTime &sample);

    /// Clear the sample date; resume live "now" tracking.
    void clearSampleDate();

    QString formatString() const;

private Q_SLOTS:
    void onTick();  // fired by the internal QTimer

private:
    void refresh();

    QLabel *m_label;
    QTimer *m_tickTimer;
    QString m_format;
    QDateTime m_sampleDate;  // invalid (default) = live
};

}  // namespace Corbomite
