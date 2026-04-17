// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphControlsPanel.h"
#include "CollapsibleSection.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QPalette>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace Corbomite {

GraphControlsPanel::GraphControlsPanel(QWidget *parent)
    : QFrame(parent)
{
    // No frame styling needed — lives inside a KDE sidebar tool view

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // Reset button
    m_resetButton = new QToolButton(this);
    m_resetButton->setText(i18n("Reset"));
    m_resetButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    m_resetButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_resetButton->setAutoRaise(true);
    mainLayout->addWidget(m_resetButton);

    // Search debounce timer
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(300);

    // Sections
    setupFiltersSection();
    setupDisplaySection();
    setupForcesSection();

    mainLayout->addWidget(m_filtersSection);
    mainLayout->addWidget(m_displaySection);
    mainLayout->addWidget(m_forcesSection);
    mainLayout->addStretch();

    // Connections — panel
    connect(m_resetButton, &QToolButton::clicked, this, &GraphControlsPanel::resetToDefaults);
}

void GraphControlsPanel::setupFiltersSection()
{
    m_filtersSection = new CollapsibleSection(i18n("Filters"), this);

    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_searchField = new QLineEdit(content);
    m_searchField->setPlaceholderText(i18n("Search files..."));
    m_searchField->setClearButtonEnabled(true);
    layout->addWidget(m_searchField);

    m_existingFilesOnly = new QCheckBox(i18n("Existing files only"), content);
    m_existingFilesOnly->setChecked(false);
    layout->addWidget(m_existingFilesOnly);

    m_orphansToggle = new QCheckBox(i18n("Show orphans"), content);
    m_orphansToggle->setChecked(true);
    layout->addWidget(m_orphansToggle);

    // TODO: Tags toggle — show/hide tag nodes (requires tag node support in GraphDataBuilder)
    // TODO: Attachments toggle — show/hide attachment nodes (requires attachment node support in GraphDataBuilder)

    m_filtersSection->setContentWidget(content);

    // Search with debounce — emit only after 300ms of inactivity
    connect(m_searchField, &QLineEdit::textChanged, this, [this]() {
        m_searchDebounce->start();
    });
    connect(m_searchDebounce, &QTimer::timeout, this, [this]() {
        Q_EMIT searchTextChanged(m_searchField->text());
    });

    connect(m_existingFilesOnly, &QCheckBox::toggled,
            this, &GraphControlsPanel::existingFilesOnlyChanged);
    connect(m_orphansToggle, &QCheckBox::toggled,
            this, &GraphControlsPanel::orphansToggled);
}

void GraphControlsPanel::setupDisplaySection()
{
    m_displaySection = new CollapsibleSection(i18n("Display"), this);

    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_arrowsToggle = new QCheckBox(i18n("Arrows"), content);
    m_arrowsToggle->setChecked(false);
    layout->addWidget(m_arrowsToggle);

    // Text fade threshold: 0.0–3.0 (slider 0–30, divide by 10)
    m_textFadeSlider = new QSlider(Qt::Horizontal, content);
    m_textFadeSlider->setRange(0, 30);
    m_textFadeSlider->setValue(10); // default 1.0
    m_textFadeValue = new QLabel(QStringLiteral("1.0"), content);
    layout->addWidget(createSliderRow(i18n("Text fade"), m_textFadeSlider, m_textFadeValue));

    // Node size: 0.5–3.0 (slider 5–30, divide by 10)
    m_nodeSizeSlider = new QSlider(Qt::Horizontal, content);
    m_nodeSizeSlider->setRange(5, 30);
    m_nodeSizeSlider->setValue(10); // default 1.0
    m_nodeSizeValue = new QLabel(QStringLiteral("1.0"), content);
    layout->addWidget(createSliderRow(i18n("Node size"), m_nodeSizeSlider, m_nodeSizeValue));

    // Link thickness: 0.5–3.0 (slider 5–30, divide by 10)
    m_linkThicknessSlider = new QSlider(Qt::Horizontal, content);
    m_linkThicknessSlider->setRange(5, 30);
    m_linkThicknessSlider->setValue(10); // default 1.0
    m_linkThicknessValue = new QLabel(QStringLiteral("1.0"), content);
    layout->addWidget(createSliderRow(i18n("Link thickness"), m_linkThicknessSlider, m_linkThicknessValue));

    m_zoomToFitButton = new QPushButton(i18n("Zoom to Fit"), content);
    m_zoomToFitButton->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    layout->addWidget(m_zoomToFitButton);

    m_animateButton = new QPushButton(i18n("Animate"), content);
    m_animateButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    layout->addWidget(m_animateButton);

    m_displaySection->setContentWidget(content);

    // Connections
    connect(m_arrowsToggle, &QCheckBox::toggled,
            this, &GraphControlsPanel::arrowsToggled);

    connect(m_textFadeSlider, &QSlider::valueChanged, this, [this](int value) {
        double threshold = value / 10.0;
        m_textFadeValue->setText(QString::number(threshold, 'f', 1));
        Q_EMIT textFadeThresholdChanged(threshold);
    });

    connect(m_nodeSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        double scale = value / 10.0;
        m_nodeSizeValue->setText(QString::number(scale, 'f', 1));
        Q_EMIT nodeSizeScaleChanged(scale);
    });

    connect(m_linkThicknessSlider, &QSlider::valueChanged, this, [this](int value) {
        double scale = value / 10.0;
        m_linkThicknessValue->setText(QString::number(scale, 'f', 1));
        Q_EMIT linkThicknessScaleChanged(scale);
    });

    connect(m_zoomToFitButton, &QPushButton::clicked,
            this, &GraphControlsPanel::zoomToFitRequested);
    connect(m_animateButton, &QPushButton::clicked,
            this, &GraphControlsPanel::animateRequested);
}

void GraphControlsPanel::setupForcesSection()
{
    m_forcesSection = new CollapsibleSection(i18n("Forces"), this);

    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Centre force: 0.0–0.05 (slider 0–50, divide by 1000)
    m_centerForceSlider = new QSlider(Qt::Horizontal, content);
    m_centerForceSlider->setRange(0, 50);
    m_centerForceSlider->setValue(10); // default 0.01
    m_centerForceValue = new QLabel(QStringLiteral("0.010"), content);
    layout->addWidget(createSliderRow(i18n("Centre force"), m_centerForceSlider, m_centerForceValue));

    // Repel force: 0–5000 (slider direct integer)
    m_repelForceSlider = new QSlider(Qt::Horizontal, content);
    m_repelForceSlider->setRange(0, 5000);
    m_repelForceSlider->setValue(1500); // default 1500
    m_repelForceValue = new QLabel(QStringLiteral("1500"), content);
    layout->addWidget(createSliderRow(i18n("Repel force"), m_repelForceSlider, m_repelForceValue));

    // Link force: 0.0–0.2 (slider 0–200, divide by 1000)
    m_linkForceSlider = new QSlider(Qt::Horizontal, content);
    m_linkForceSlider->setRange(0, 200);
    m_linkForceSlider->setValue(50); // default 0.05
    m_linkForceValue = new QLabel(QStringLiteral("0.050"), content);
    layout->addWidget(createSliderRow(i18n("Link force"), m_linkForceSlider, m_linkForceValue));

    // Link distance: 20–300 (slider direct integer)
    m_linkDistanceSlider = new QSlider(Qt::Horizontal, content);
    m_linkDistanceSlider->setRange(20, 300);
    m_linkDistanceSlider->setValue(100); // default 100
    m_linkDistanceValue = new QLabel(QStringLiteral("100"), content);
    layout->addWidget(createSliderRow(i18n("Link distance"), m_linkDistanceSlider, m_linkDistanceValue));

    m_forcesSection->setContentWidget(content);

    // Connections
    connect(m_centerForceSlider, &QSlider::valueChanged, this, [this](int value) {
        double force = value / 1000.0;
        m_centerForceValue->setText(QString::number(force, 'f', 3));
        Q_EMIT centerForceChanged(force);
    });

    connect(m_repelForceSlider, &QSlider::valueChanged, this, [this](int value) {
        m_repelForceValue->setText(QString::number(value));
        Q_EMIT repelForceChanged(static_cast<double>(value));
    });

    connect(m_linkForceSlider, &QSlider::valueChanged, this, [this](int value) {
        double force = value / 1000.0;
        m_linkForceValue->setText(QString::number(force, 'f', 3));
        Q_EMIT linkForceChanged(force);
    });

    connect(m_linkDistanceSlider, &QSlider::valueChanged, this, [this](int value) {
        m_linkDistanceValue->setText(QString::number(value));
        Q_EMIT linkDistanceChanged(static_cast<double>(value));
    });
}

QWidget *GraphControlsPanel::createSliderRow(const QString &labelText, QSlider *slider, QLabel *valueLabel)
{
    auto *row = new QWidget(this);
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    auto *label = new QLabel(labelText, row);
    QFont smallFont = label->font();
    smallFont.setPointSize(smallFont.pointSize() - 1);
    label->setFont(smallFont);
    topRow->addWidget(label);
    topRow->addStretch();
    valueLabel->setFont(smallFont);
    valueLabel->setForegroundRole(QPalette::PlaceholderText);
    valueLabel->setFixedWidth(40);
    valueLabel->setAlignment(Qt::AlignRight);
    topRow->addWidget(valueLabel);

    layout->addLayout(topRow);
    layout->addWidget(slider);

    return row;
}

// --- Getters ---

QString GraphControlsPanel::searchText() const
{
    return m_searchField->text();
}

bool GraphControlsPanel::existingFilesOnly() const
{
    return m_existingFilesOnly->isChecked();
}

bool GraphControlsPanel::showOrphans() const
{
    return m_orphansToggle->isChecked();
}

bool GraphControlsPanel::showArrows() const
{
    return m_arrowsToggle->isChecked();
}

double GraphControlsPanel::textFadeThreshold() const
{
    return m_textFadeSlider->value() / 10.0;
}

double GraphControlsPanel::nodeSizeScale() const
{
    return m_nodeSizeSlider->value() / 10.0;
}

double GraphControlsPanel::linkThicknessScale() const
{
    return m_linkThicknessSlider->value() / 10.0;
}

double GraphControlsPanel::centerForce() const
{
    return m_centerForceSlider->value() / 1000.0;
}

double GraphControlsPanel::repelForce() const
{
    return static_cast<double>(m_repelForceSlider->value());
}

double GraphControlsPanel::linkForce() const
{
    return m_linkForceSlider->value() / 1000.0;
}

double GraphControlsPanel::linkDistance() const
{
    return static_cast<double>(m_linkDistanceSlider->value());
}

// --- Reset ---

void GraphControlsPanel::resetToDefaults()
{
    // Block signals during reset to avoid intermediate rebuilds
    m_searchField->blockSignals(true);
    m_existingFilesOnly->blockSignals(true);
    m_orphansToggle->blockSignals(true);
    m_arrowsToggle->blockSignals(true);
    m_textFadeSlider->blockSignals(true);
    m_nodeSizeSlider->blockSignals(true);
    m_linkThicknessSlider->blockSignals(true);
    m_centerForceSlider->blockSignals(true);
    m_repelForceSlider->blockSignals(true);
    m_linkForceSlider->blockSignals(true);
    m_linkDistanceSlider->blockSignals(true);

    // Filters
    m_searchField->clear();
    m_existingFilesOnly->setChecked(false);
    m_orphansToggle->setChecked(true);

    // Display
    m_arrowsToggle->setChecked(false);
    m_textFadeSlider->setValue(10);
    m_textFadeValue->setText(QStringLiteral("1.0"));
    m_nodeSizeSlider->setValue(10);
    m_nodeSizeValue->setText(QStringLiteral("1.0"));
    m_linkThicknessSlider->setValue(10);
    m_linkThicknessValue->setText(QStringLiteral("1.0"));

    // Forces
    m_centerForceSlider->setValue(10);
    m_centerForceValue->setText(QStringLiteral("0.010"));
    m_repelForceSlider->setValue(1500);
    m_repelForceValue->setText(QStringLiteral("1500"));
    m_linkForceSlider->setValue(50);
    m_linkForceValue->setText(QStringLiteral("0.050"));
    m_linkDistanceSlider->setValue(100);
    m_linkDistanceValue->setText(QStringLiteral("100"));

    // Unblock signals
    m_searchField->blockSignals(false);
    m_existingFilesOnly->blockSignals(false);
    m_orphansToggle->blockSignals(false);
    m_arrowsToggle->blockSignals(false);
    m_textFadeSlider->blockSignals(false);
    m_nodeSizeSlider->blockSignals(false);
    m_linkThicknessSlider->blockSignals(false);
    m_centerForceSlider->blockSignals(false);
    m_repelForceSlider->blockSignals(false);
    m_linkForceSlider->blockSignals(false);
    m_linkDistanceSlider->blockSignals(false);

    // Emit all changed signals once with default values
    Q_EMIT searchTextChanged(QString());
    Q_EMIT existingFilesOnlyChanged(false);
    Q_EMIT orphansToggled(true);
    Q_EMIT arrowsToggled(false);
    Q_EMIT textFadeThresholdChanged(1.0);
    Q_EMIT nodeSizeScaleChanged(1.0);
    Q_EMIT linkThicknessScaleChanged(1.0);
    Q_EMIT centerForceChanged(0.01);
    Q_EMIT repelForceChanged(1500.0);
    Q_EMIT linkForceChanged(0.05);
    Q_EMIT linkDistanceChanged(100.0);
}

} // namespace Corbomite
