// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>

class QLineEdit;
class QCheckBox;
class QSlider;
class QPushButton;
class QToolButton;
class QLabel;

namespace Corbomite {

class CollapsibleSection;

class GraphControlsPanel : public QFrame {
    Q_OBJECT

public:
    explicit GraphControlsPanel(QWidget *parent = nullptr);

    // Reset all controls to default values
    void resetToDefaults();

    // Current values — Filters
    QString searchText() const;
    bool existingFilesOnly() const;
    bool showOrphans() const;

    // Current values — Display
    bool showArrows() const;
    double textFadeThreshold() const;  // 0.0–3.0
    double nodeSizeScale() const;      // 0.5–3.0
    double linkThicknessScale() const; // 0.5–3.0

    // Current values — Forces
    double centerForce() const;   // 0.0–0.05
    double repelForce() const;    // 0–5000
    double linkForce() const;     // 0.0–0.2
    double linkDistance() const;  // 20–300

Q_SIGNALS:
    // Filters
    void searchTextChanged(const QString &text);
    void existingFilesOnlyChanged(bool checked);
    void orphansToggled(bool show);

    // Display
    void arrowsToggled(bool show);
    void textFadeThresholdChanged(double threshold);
    void nodeSizeScaleChanged(double scale);
    void linkThicknessScaleChanged(double scale);
    void zoomToFitRequested();
    void animateRequested();

    // Forces
    void centerForceChanged(double value);
    void repelForceChanged(double value);
    void linkForceChanged(double value);
    void linkDistanceChanged(double value);

    // Panel
    void closeRequested();

private:
    void setupFiltersSection();
    void setupDisplaySection();
    void setupForcesSection();
    QWidget *createSliderRow(const QString &labelText, QSlider *slider, QLabel *valueLabel);

    // Filters
    CollapsibleSection *m_filtersSection;
    QLineEdit *m_searchField;
    QCheckBox *m_existingFilesOnly;
    QCheckBox *m_orphansToggle;

    // Display
    CollapsibleSection *m_displaySection;
    QCheckBox *m_arrowsToggle;
    QSlider *m_textFadeSlider;
    QLabel *m_textFadeValue;
    QSlider *m_nodeSizeSlider;
    QLabel *m_nodeSizeValue;
    QSlider *m_linkThicknessSlider;
    QLabel *m_linkThicknessValue;
    QPushButton *m_zoomToFitButton;
    QPushButton *m_animateButton;

    // Forces
    CollapsibleSection *m_forcesSection;
    QSlider *m_centerForceSlider;
    QLabel *m_centerForceValue;
    QSlider *m_repelForceSlider;
    QLabel *m_repelForceValue;
    QSlider *m_linkForceSlider;
    QLabel *m_linkForceValue;
    QSlider *m_linkDistanceSlider;
    QLabel *m_linkDistanceValue;

    // Panel controls
    QToolButton *m_resetButton;
    QToolButton *m_closeButton;

    QTimer *m_searchDebounce;
};

} // namespace Corbomite
