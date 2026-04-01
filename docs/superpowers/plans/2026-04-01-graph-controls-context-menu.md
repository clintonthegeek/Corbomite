# Graph View Controls Panel & Context Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a floating controls panel (Filters, Display, Forces) and right-click context menu to the graph view, making it interactive and configurable.

**Architecture:** New `GraphControlsPanel` widget floats over the top-right of `ForceGraphView`. Slider changes call existing `ForceLayoutEngine` setters. Filter changes rebuild the visible node set via `GraphViewTab`. Display settings (node size, edge width, arrows, text threshold) propagate to `ForceGraphScene`. Right-click context menu on nodes emits signals up to MainWindow.

**Tech Stack:** C++20, Qt6 (QGraphicsView, QSlider, QCheckBox, QMenu), KDE Frameworks 6 (KLocalizedString, KMessageBox)

**Spec:** `docs/superpowers/specs/2026-04-01-graph-controls-context-menu-design.md`

---

### Task 1: CollapsibleSection Helper Widget

**Files:**
- Create: `src/graph/CollapsibleSection.h`
- Create: `src/graph/CollapsibleSection.cpp`
- Modify: `src/CMakeLists.txt`

A reusable collapsible section widget: clickable arrow header toggles content visibility. Used by the controls panel for Filters, Display, and Forces sections.

- [ ] **Step 1: Create CollapsibleSection.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QToolButton;
class QVBoxLayout;

namespace Corbomite {

class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr);

    void setContentWidget(QWidget *content);
    QWidget *contentWidget() const;
    void setExpanded(bool expanded);
    bool isExpanded() const;

public Q_SLOTS:
    void toggle();

private:
    QToolButton *m_headerButton;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_layout;
    bool m_expanded = false;
};

} // namespace Corbomite
```

- [ ] **Step 2: Create CollapsibleSection.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CollapsibleSection.h"

#include <QToolButton>
#include <QVBoxLayout>

namespace Corbomite {

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_headerButton = new QToolButton(this);
    m_headerButton->setText(title);
    m_headerButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_headerButton->setArrowType(Qt::RightArrow);
    m_headerButton->setCheckable(true);
    m_headerButton->setChecked(false);
    m_headerButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerButton->setStyleSheet(
        QStringLiteral("QToolButton { border: none; font-weight: bold; padding: 4px; }")
    );

    m_layout->addWidget(m_headerButton);

    connect(m_headerButton, &QToolButton::toggled, this, [this](bool checked) {
        setExpanded(checked);
    });
}

void CollapsibleSection::setContentWidget(QWidget *content)
{
    if (m_content) {
        m_layout->removeWidget(m_content);
        m_content->setParent(nullptr);
    }

    m_content = content;
    if (m_content) {
        m_layout->addWidget(m_content);
        m_content->setVisible(m_expanded);
    }
}

QWidget *CollapsibleSection::contentWidget() const
{
    return m_content;
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_headerButton->setChecked(expanded);
    m_headerButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    if (m_content) {
        m_content->setVisible(expanded);
    }
}

bool CollapsibleSection::isExpanded() const
{
    return m_expanded;
}

void CollapsibleSection::toggle()
{
    setExpanded(!m_expanded);
}

} // namespace Corbomite
```

- [ ] **Step 3: Add to CMakeLists.txt**

In `src/CMakeLists.txt`, add `graph/CollapsibleSection.cpp` to the `CorbomiteApp` source list:

```cmake
    graph/GraphDataBuilder.cpp
    graph/GraphViewTab.cpp
    graph/LocalGraphPanel.cpp
    graph/CollapsibleSection.cpp
    canvas/CanvasViewTab.cpp
```

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

---

### Task 2: GraphControlsPanel Widget

**Files:**
- Create: `src/graph/GraphControlsPanel.h`
- Create: `src/graph/GraphControlsPanel.cpp`
- Modify: `src/CMakeLists.txt`

The floating panel with three collapsible sections (Filters, Display, Forces). Emits signals for all control changes. Does not wire anything yet — that's Task 4.

- [ ] **Step 1: Create GraphControlsPanel.h**

```cpp
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
```

- [ ] **Step 2: Create GraphControlsPanel.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphControlsPanel.h"
#include "CollapsibleSection.h"

#include <KLocalizedString>

#include <QCheckBox>
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
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setFixedWidth(220);

    // Semi-transparent background with rounded corners
    setStyleSheet(QStringLiteral(
        "GraphControlsPanel {"
        "  background-color: rgba(30, 30, 30, 230);"
        "  border-radius: 8px;"
        "  border: 1px solid rgba(255, 255, 255, 30);"
        "}"
    ));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // Header row: title + reset + close
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(i18n("Graph Controls"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px;"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_resetButton = new QToolButton(this);
    m_resetButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    m_resetButton->setToolTip(i18n("Reset all controls to defaults"));
    m_resetButton->setAutoRaise(true);
    headerLayout->addWidget(m_resetButton);

    m_closeButton = new QToolButton(this);
    m_closeButton->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    m_closeButton->setToolTip(i18n("Close panel"));
    m_closeButton->setAutoRaise(true);
    headerLayout->addWidget(m_closeButton);

    mainLayout->addLayout(headerLayout);

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
    connect(m_closeButton, &QToolButton::clicked, this, &GraphControlsPanel::closeRequested);
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
    label->setStyleSheet(QStringLiteral("font-size: 11px;"));
    topRow->addWidget(label);
    topRow->addStretch();
    valueLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #aaa;"));
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
```

- [ ] **Step 3: Add GraphControlsPanel.cpp to CMakeLists.txt**

In `src/CMakeLists.txt`, add `graph/GraphControlsPanel.cpp` to the source list:

```cmake
    graph/GraphDataBuilder.cpp
    graph/GraphViewTab.cpp
    graph/LocalGraphPanel.cpp
    graph/CollapsibleSection.cpp
    graph/GraphControlsPanel.cpp
    canvas/CanvasViewTab.cpp
```

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

---

### Task 3: Wire Display Settings into ForceGraphScene/Node/Edge

**Files:**
- Modify: `libs/forcegraph/include/forcegraph/ForceGraphScene.h`
- Modify: `libs/forcegraph/src/ForceGraphScene.cpp`
- Modify: `libs/forcegraph/include/forcegraph/ForceGraphNode.h`
- Modify: `libs/forcegraph/src/ForceGraphNode.cpp`
- Modify: `libs/forcegraph/include/forcegraph/ForceGraphEdge.h`
- Modify: `libs/forcegraph/src/ForceGraphEdge.cpp`

Add display setting support to the forcegraph library: node size scale, edge width scale, text fade threshold, and arrows toggle. These are Qt-only changes (no KDE deps). The scene propagates settings down to its node/edge items.

- [ ] **Step 1: Add display methods to ForceGraphNode**

In `libs/forcegraph/include/forcegraph/ForceGraphNode.h`, add setters for display settings:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsEllipseItem>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode : public QGraphicsEllipseItem {
public:
    explicit ForceGraphNode(const GraphNode &data, QGraphicsItem *parent = nullptr);
    void setData(const GraphNode &data);
    QString nodeId() const;
    QString nodeLabel() const;
    void setHighlighted(bool highlighted);
    void setDimmed(bool dimmed);
    void setNodeSizeScale(double scale);
    void setTextFadeThreshold(double threshold);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
private:
    GraphNode m_data;
    bool m_highlighted = false;
    bool m_dimmed = false;
    double m_sizeScale = 1.0;
    double m_textFadeThreshold = 1.0;
};
} // namespace ForceGraph
```

- [ ] **Step 2: Implement ForceGraphNode display settings**

In `libs/forcegraph/src/ForceGraphNode.cpp`:

Add the `nodeLabel()` getter:

```cpp
QString ForceGraphNode::nodeLabel() const
{
    return m_data.label;
}
```

Add the `setNodeSizeScale()` method. It recalculates the ellipse rect using the original data radius times the scale factor:

```cpp
void ForceGraphNode::setNodeSizeScale(double scale)
{
    m_sizeScale = scale;
    double r = m_data.radius * m_sizeScale;
    setRect(-r, -r, 2 * r, 2 * r);
    update();
}
```

Add the `setTextFadeThreshold()` method:

```cpp
void ForceGraphNode::setTextFadeThreshold(double threshold)
{
    m_textFadeThreshold = threshold;
    update();
}
```

Update the `setData()` method to apply the current size scale:

```cpp
void ForceGraphNode::setData(const GraphNode &data)
{
    m_data = data;
    double r = m_data.radius * m_sizeScale;
    setRect(-r, -r, 2 * r, 2 * r);
}
```

Update `paint()` to use `m_textFadeThreshold` instead of hardcoded `1.0`. Replace the medium-detail label block:

```cpp
    if (lod < 2.0) {
        // Medium: abbreviated label only when zoomed in past the threshold
        if (lod >= m_textFadeThreshold && !m_data.label.isEmpty()) {
            painter->setPen(m_dimmed ? QColor(128, 128, 128, 40) : QColor(80, 80, 80));
            QFont font;
            font.setPointSizeF(6.0);
            painter->setFont(font);
            painter->drawText(QPointF(-m_data.radius, m_data.radius + 10),
                              m_data.label.left(12));
        }
        return;
    }
```

- [ ] **Step 3: Add display methods to ForceGraphEdge**

In `libs/forcegraph/include/forcegraph/ForceGraphEdge.h`, add setters:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsLineItem>
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge : public QGraphicsLineItem {
public:
    ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent = nullptr);
    void adjust();
    ForceGraphNode *sourceNode() const;
    ForceGraphNode *targetNode() const;
    void setDimmed(bool dimmed);
    void setWidthScale(double scale);
    void setShowArrows(bool show);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
private:
    void updatePen();
    ForceGraphNode *m_source;
    ForceGraphNode *m_target;
    bool m_dimmed = false;
    double m_widthScale = 1.0;
    bool m_showArrows = false;
};
} // namespace ForceGraph
```

- [ ] **Step 4: Implement ForceGraphEdge display settings**

In `libs/forcegraph/src/ForceGraphEdge.cpp`, rewrite to support width scale and arrows:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "forcegraph/ForceGraphEdge.h"
#include "forcegraph/ForceGraphNode.h"
#include <QPen>
#include <QPainter>
#include <QtMath>

namespace ForceGraph {

ForceGraphEdge::ForceGraphEdge(ForceGraphNode *source, ForceGraphNode *target, QGraphicsItem *parent)
    : QGraphicsLineItem(parent)
    , m_source(source)
    , m_target(target)
{
    setZValue(0); // Behind nodes
    updatePen();
    adjust();
}

void ForceGraphEdge::adjust()
{
    if (!m_source || !m_target) return;
    setLine(QLineF(m_source->pos(), m_target->pos()));
}

ForceGraphNode *ForceGraphEdge::sourceNode() const { return m_source; }
ForceGraphNode *ForceGraphEdge::targetNode() const { return m_target; }

void ForceGraphEdge::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    updatePen();
}

void ForceGraphEdge::setWidthScale(double scale)
{
    m_widthScale = scale;
    updatePen();
}

void ForceGraphEdge::setShowArrows(bool show)
{
    m_showArrows = show;
    update();
}

void ForceGraphEdge::updatePen()
{
    if (m_dimmed) {
        setPen(QPen(QColor(200, 200, 200, 30), 0.5 * m_widthScale));
    } else {
        setPen(QPen(QColor(150, 150, 150, 100), 1.0 * m_widthScale));
    }
}

void ForceGraphEdge::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Draw the line
    QGraphicsLineItem::paint(painter, option, widget);

    // Draw arrowhead at target end if enabled
    if (!m_showArrows || m_dimmed) return;

    QLineF edgeLine = line();
    if (edgeLine.length() < 1.0) return;

    double arrowSize = 6.0 * m_widthScale;
    double angle = std::atan2(-edgeLine.dy(), edgeLine.dx());

    QPointF targetPoint = edgeLine.p2();
    QPointF arrowP1 = targetPoint + QPointF(
        std::sin(angle - M_PI / 3) * arrowSize,
        std::cos(angle - M_PI / 3) * arrowSize);
    QPointF arrowP2 = targetPoint + QPointF(
        std::sin(angle - M_PI + M_PI / 3) * arrowSize,
        std::cos(angle - M_PI + M_PI / 3) * arrowSize);

    painter->setPen(Qt::NoPen);
    painter->setBrush(pen().color());
    QPolygonF arrowHead;
    arrowHead << targetPoint << arrowP1 << arrowP2;
    painter->drawPolygon(arrowHead);
}

} // namespace ForceGraph
```

- [ ] **Step 5: Add display methods to ForceGraphScene**

In `libs/forcegraph/include/forcegraph/ForceGraphScene.h`, add methods that propagate display settings to all items:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsScene>
#include <QHash>
#include "GraphTypes.h"
namespace ForceGraph {
class ForceGraphNode;
class ForceGraphEdge;
class ForceGraphScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit ForceGraphScene(QObject *parent = nullptr);
    void setNodes(const QVector<GraphNode> &nodes);
    void setEdges(const QVector<GraphEdge> &edges);
    void updatePositions(const QHash<QString, QPointF> &positions);
    void setHighlightedNode(const QString &id);
    void clearHighlight();
    ForceGraphNode *nodeItem(const QString &id) const;

    // Display settings — propagate to all items
    void setNodeSizeScale(double scale);
    void setEdgeWidthScale(double scale);
    void setTextFadeThreshold(double threshold);
    void setShowArrows(bool show);

private:
    QHash<QString, ForceGraphNode *> m_nodeItems;
    QVector<ForceGraphEdge *> m_edgeItems;
    QString m_highlightedId;

    // Cached display settings (applied to newly created items too)
    double m_nodeSizeScale = 1.0;
    double m_edgeWidthScale = 1.0;
    double m_textFadeThreshold = 1.0;
    bool m_showArrows = false;
};
} // namespace ForceGraph
```

- [ ] **Step 6: Implement ForceGraphScene display settings**

In `libs/forcegraph/src/ForceGraphScene.cpp`, add the four new methods:

```cpp
void ForceGraphScene::setNodeSizeScale(double scale)
{
    m_nodeSizeScale = scale;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setNodeSizeScale(scale);
    }
}

void ForceGraphScene::setEdgeWidthScale(double scale)
{
    m_edgeWidthScale = scale;
    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setWidthScale(scale);
    }
}

void ForceGraphScene::setTextFadeThreshold(double threshold)
{
    m_textFadeThreshold = threshold;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        it.value()->setTextFadeThreshold(threshold);
    }
}

void ForceGraphScene::setShowArrows(bool show)
{
    m_showArrows = show;
    for (auto *edge : std::as_const(m_edgeItems)) {
        edge->setShowArrows(show);
    }
}
```

Also update `setNodes()` to apply cached display settings to newly created nodes:

```cpp
void ForceGraphScene::setNodes(const QVector<GraphNode> &nodes)
{
    clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_highlightedId.clear();

    for (const auto &node : nodes) {
        auto *item = new ForceGraphNode(node);
        item->setNodeSizeScale(m_nodeSizeScale);
        item->setTextFadeThreshold(m_textFadeThreshold);
        addItem(item);
        m_nodeItems.insert(node.id, item);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (nodes.size() > 200) {
        setMinimumRenderSize(1.0);
    }
#endif
}
```

And update `setEdges()` to apply cached settings:

```cpp
void ForceGraphScene::setEdges(const QVector<GraphEdge> &edges)
{
    for (auto *edge : std::as_const(m_edgeItems)) {
        removeItem(edge);
        delete edge;
    }
    m_edgeItems.clear();

    for (const auto &edge : edges) {
        auto *sourceItem = m_nodeItems.value(edge.sourceId);
        auto *targetItem = m_nodeItems.value(edge.targetId);
        if (!sourceItem || !targetItem)
            continue;

        auto *item = new ForceGraphEdge(sourceItem, targetItem);
        item->setWidthScale(m_edgeWidthScale);
        item->setShowArrows(m_showArrows);
        addItem(item);
        m_edgeItems.append(item);
    }
}
```

- [ ] **Step 7: Add forwarding methods to ForceGraphView**

Since `ForceGraphView::m_scene` is private, add forwarding methods in `libs/forcegraph/include/forcegraph/ForceGraphView.h`:

```cpp
    // Display settings — forwarded to scene
    void setNodeSizeScale(double scale);
    void setEdgeWidthScale(double scale);
    void setTextFadeThreshold(double threshold);
    void setShowArrows(bool show);
```

And in `libs/forcegraph/src/ForceGraphView.cpp`:

```cpp
void ForceGraphView::setNodeSizeScale(double scale)
{
    m_scene->setNodeSizeScale(scale);
}

void ForceGraphView::setEdgeWidthScale(double scale)
{
    m_scene->setEdgeWidthScale(scale);
}

void ForceGraphView::setTextFadeThreshold(double threshold)
{
    m_scene->setTextFadeThreshold(threshold);
}

void ForceGraphView::setShowArrows(bool show)
{
    m_scene->setShowArrows(show);
}
```

- [ ] **Step 8: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

---

### Task 4: Wire Controls Panel into GraphViewTab

**Files:**
- Modify: `src/graph/GraphViewTab.h`
- Modify: `src/graph/GraphViewTab.cpp`

Integrate the panel: position it floating over ForceGraphView, connect force sliders to engine, display sliders to scene (via view forwarding methods), and implement filter logic (search dimming, orphan/unresolved toggles). The toggle button shows/hides the panel.

- [ ] **Step 1: Update GraphViewTab.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <forcegraph/GraphTypes.h>

class QToolButton;

namespace ForceGraph {
class ForceLayoutEngine;
class ForceGraphView;
}

namespace Corbomite {

class SQLiteIndex;
class VaultModel;
class GraphControlsPanel;

class GraphViewTab : public QWidget {
    Q_OBJECT

public:
    explicit GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent = nullptr);
    ~GraphViewTab() override;

    void buildGraph();

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupControlsPanel();
    void connectForceControls();
    void connectDisplayControls();
    void connectFilterControls();
    void applyFilters();
    void positionPanel();
    void applySearchDimming(const QString &text);
    void restartSimulation();

    ForceGraph::ForceGraphView *m_graphView;
    ForceGraph::ForceLayoutEngine *m_engine;
    SQLiteIndex *m_index;
    VaultModel *m_vault;

    GraphControlsPanel *m_controlsPanel = nullptr;
    QToolButton *m_toggleButton = nullptr;

    // Full graph data before filtering
    QVector<ForceGraph::GraphNode> m_allNodes;
    QVector<ForceGraph::GraphEdge> m_allEdges;
};

} // namespace Corbomite
```

- [ ] **Step 2: Rewrite GraphViewTab.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewTab.h"
#include "GraphControlsPanel.h"
#include "GraphDataBuilder.h"

#include <forcegraph/ForceLayoutEngine.h>
#include <forcegraph/ForceGraphView.h>

#include <KLocalizedString>

#include <QIcon>
#include <QResizeEvent>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace Corbomite {

GraphViewTab::GraphViewTab(SQLiteIndex *index, VaultModel *vault, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_vault(vault)
{
    m_engine = new ForceGraph::ForceLayoutEngine(this);
    m_graphView = new ForceGraph::ForceGraphView(this);
    m_graphView->setEngine(m_engine);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_graphView);

    connect(m_graphView, &ForceGraph::ForceGraphView::nodeClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });
    connect(m_graphView, &ForceGraph::ForceGraphView::nodeDoubleClicked,
            this, [this](const QString &id) {
        Q_EMIT noteActivated(id);
    });

    setupControlsPanel();
    buildGraph();
}

GraphViewTab::~GraphViewTab()
{
    m_engine->stop();
}

void GraphViewTab::setupControlsPanel()
{
    // Controls panel — floats over the graph view
    m_controlsPanel = new GraphControlsPanel(m_graphView);
    m_controlsPanel->hide(); // Start hidden

    // Toggle button — shown when panel is hidden
    m_toggleButton = new QToolButton(m_graphView);
    m_toggleButton->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    m_toggleButton->setToolTip(i18n("Show graph controls"));
    m_toggleButton->setAutoRaise(true);
    m_toggleButton->setFixedSize(32, 32);
    m_toggleButton->show();

    connect(m_toggleButton, &QToolButton::clicked, this, [this]() {
        m_controlsPanel->show();
        m_toggleButton->hide();
        positionPanel();
    });

    connect(m_controlsPanel, &GraphControlsPanel::closeRequested, this, [this]() {
        m_controlsPanel->hide();
        m_toggleButton->show();
        positionPanel();
    });

    connectForceControls();
    connectDisplayControls();
    connectFilterControls();
}

void GraphViewTab::connectForceControls()
{
    connect(m_controlsPanel, &GraphControlsPanel::centerForceChanged,
            this, [this](double value) {
        m_engine->setCenterForce(value);
        restartSimulation();
    });
    connect(m_controlsPanel, &GraphControlsPanel::repelForceChanged,
            this, [this](double value) {
        m_engine->setRepelForce(value);
        restartSimulation();
    });
    connect(m_controlsPanel, &GraphControlsPanel::linkForceChanged,
            this, [this](double value) {
        m_engine->setLinkForce(value);
        restartSimulation();
    });
    connect(m_controlsPanel, &GraphControlsPanel::linkDistanceChanged,
            this, [this](double value) {
        m_engine->setLinkDistance(value);
        restartSimulation();
    });
}

void GraphViewTab::connectDisplayControls()
{
    connect(m_controlsPanel, &GraphControlsPanel::nodeSizeScaleChanged,
            m_graphView, &ForceGraph::ForceGraphView::setNodeSizeScale);

    connect(m_controlsPanel, &GraphControlsPanel::linkThicknessScaleChanged,
            m_graphView, &ForceGraph::ForceGraphView::setEdgeWidthScale);

    connect(m_controlsPanel, &GraphControlsPanel::textFadeThresholdChanged,
            m_graphView, &ForceGraph::ForceGraphView::setTextFadeThreshold);

    connect(m_controlsPanel, &GraphControlsPanel::arrowsToggled,
            m_graphView, &ForceGraph::ForceGraphView::setShowArrows);

    connect(m_controlsPanel, &GraphControlsPanel::animateRequested,
            this, [this]() {
        // Re-randomize positions and restart
        m_engine->stop();
        // Clear pinned state so nodes get new random positions
        for (const auto &node : m_allNodes) {
            m_engine->unpinNode(node.id);
        }
        restartSimulation();
    });
}

void GraphViewTab::connectFilterControls()
{
    connect(m_controlsPanel, &GraphControlsPanel::searchTextChanged,
            this, &GraphViewTab::applySearchDimming);

    connect(m_controlsPanel, &GraphControlsPanel::existingFilesOnlyChanged,
            this, [this]() { applyFilters(); });

    connect(m_controlsPanel, &GraphControlsPanel::orphansToggled,
            this, [this]() { applyFilters(); });
}

void GraphViewTab::buildGraph()
{
    auto data = GraphDataBuilder::buildGlobalGraph(m_index, m_vault);

    // Cap node count for performance — 6000+ node graphs freeze the UI.
    // TODO: Implement multilevel coarsening (Handbook Ch. 12.6) for large graphs.
    static constexpr int MAX_GRAPH_NODES = 1000;
    if (data.nodes.size() > MAX_GRAPH_NODES) {
        qWarning() << "Graph too large:" << data.nodes.size() << "nodes. Capping at" << MAX_GRAPH_NODES;
        // Keep only the most connected nodes
        std::sort(data.nodes.begin(), data.nodes.end(),
                  [](const ForceGraph::GraphNode &a, const ForceGraph::GraphNode &b) {
                      return a.radius > b.radius; // radius encodes degree
                  });
        QSet<QString> kept;
        for (int i = 0; i < MAX_GRAPH_NODES && i < data.nodes.size(); ++i) {
            kept.insert(data.nodes[i].id);
        }
        data.nodes.resize(MAX_GRAPH_NODES);

        // Filter edges to only include kept nodes
        QVector<ForceGraph::GraphEdge> filteredEdges;
        for (const auto &edge : data.edges) {
            if (kept.contains(edge.sourceId) && kept.contains(edge.targetId)) {
                filteredEdges.append(edge);
            }
        }
        data.edges = filteredEdges;
    }

    // Store full graph for filtering
    m_allNodes = data.nodes;
    m_allEdges = data.edges;

    applyFilters();
}

void GraphViewTab::applyFilters()
{
    bool existingOnly = m_controlsPanel ? m_controlsPanel->existingFilesOnly() : false;
    bool showOrphans = m_controlsPanel ? m_controlsPanel->showOrphans() : true;

    // Determine which node IDs have connections
    QHash<QString, int> degree;
    for (const auto &node : m_allNodes) {
        degree[node.id] = 0;
    }
    for (const auto &edge : m_allEdges) {
        degree[edge.sourceId]++;
        degree[edge.targetId]++;
    }

    // Filter nodes
    QVector<ForceGraph::GraphNode> filteredNodes;
    QSet<QString> keptIds;

    for (const auto &node : m_allNodes) {
        // Unresolved node detection: gray color with radius 3.0
        // (matches GraphDataBuilder unresolved node creation)
        bool isUnresolved = (node.radius == 3.0 && node.color == QColor(136, 136, 136));
        bool isOrphan = (degree.value(node.id, 0) == 0);

        if (existingOnly && isUnresolved) continue;
        if (!showOrphans && isOrphan) continue;

        filteredNodes.append(node);
        keptIds.insert(node.id);
    }

    // Filter edges to only reference kept nodes
    QVector<ForceGraph::GraphEdge> filteredEdges;
    for (const auto &edge : m_allEdges) {
        if (keptIds.contains(edge.sourceId) && keptIds.contains(edge.targetId)) {
            filteredEdges.append(edge);
        }
    }

    m_engine->stop();
    m_graphView->setNodes(filteredNodes);
    m_graphView->setEdges(filteredEdges);
    m_graphView->zoomToFit();
    m_engine->start();

    connect(m_engine, &ForceGraph::ForceLayoutEngine::simulationStable,
            this, [this]() {
        m_graphView->zoomToFit();
    }, Qt::SingleShotConnection);

    // Re-apply search dimming if active
    if (m_controlsPanel && !m_controlsPanel->searchText().isEmpty()) {
        applySearchDimming(m_controlsPanel->searchText());
    }
}

void GraphViewTab::applySearchDimming(const QString &text)
{
    if (text.isEmpty()) {
        m_graphView->clearHighlight();
        return;
    }

    // Dim all nodes whose label doesn't contain the search text.
    // We use the existing setHighlightedNode/clearHighlight mechanism
    // but implement manual dimming since we need multi-match, not single-node highlight.
    //
    // ForceGraphView doesn't expose per-node dimming directly, but the scene's
    // nodeItem() is accessible via the forwarding nodeItem() or we iterate.
    // For now, set highlight on the first match and let the existing system handle it.
    // A proper implementation needs a scene-level "dim by predicate" method.
    //
    // Simpler approach: iterate the view's nodes and dim non-matching ones.
    // ForceGraphView needs a method for this — add setDimmedNodes().
    // But since we're in the app layer and ForceGraphView::m_scene is private,
    // use the setNodeColor approach: recolor non-matching nodes to a dim color.
    //
    // Cleanest approach: add a searchDimming method to ForceGraphView.

    // We need to access scene items. Use ForceGraphView forwarding.
    // For the initial implementation, we'll re-apply node data with modified colors.
    // This is handled via the clearHighlight + manual approach below.

    m_graphView->clearHighlight();

    // Use setHighlightedNode for each matching node is wrong (single highlight).
    // Instead, we need to set dimmed state on non-matching nodes.
    // ForceGraphView needs a new method. Add it:
    m_graphView->setSearchFilter(text);
}

void GraphViewTab::restartSimulation()
{
    m_engine->stop();
    m_engine->start();
}

void GraphViewTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionPanel();
}

void GraphViewTab::positionPanel()
{
    if (!m_controlsPanel || !m_toggleButton) return;

    const int margin = 8;

    // Position panel at top-right of the graph view
    if (m_controlsPanel->isVisible()) {
        m_controlsPanel->move(
            m_graphView->width() - m_controlsPanel->width() - margin,
            margin
        );
    }

    // Position toggle button at top-right
    if (m_toggleButton->isVisible()) {
        m_toggleButton->move(
            m_graphView->width() - m_toggleButton->width() - margin,
            margin
        );
    }
}

} // namespace Corbomite
```

- [ ] **Step 3: Add setSearchFilter to ForceGraphView**

The `applySearchDimming` method needs ForceGraphView to support search-based dimming. Add to `libs/forcegraph/include/forcegraph/ForceGraphView.h`:

```cpp
    void setSearchFilter(const QString &text);
```

And in `libs/forcegraph/src/ForceGraphView.cpp`:

```cpp
void ForceGraphView::setSearchFilter(const QString &text)
{
    if (text.isEmpty()) {
        // Clear all dimming
        m_scene->clearHighlight();
        return;
    }

    // Dim nodes that don't match the search text
    m_scene->setSearchFilter(text);
}
```

Add the corresponding method to `ForceGraphScene`. In `libs/forcegraph/include/forcegraph/ForceGraphScene.h`:

```cpp
    void setSearchFilter(const QString &text);
```

In `libs/forcegraph/src/ForceGraphScene.cpp`:

```cpp
void ForceGraphScene::setSearchFilter(const QString &text)
{
    m_highlightedId.clear();

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        bool matches = it.value()->nodeLabel().contains(text, Qt::CaseInsensitive);
        it.value()->setHighlighted(false);
        it.value()->setDimmed(!matches);
    }

    for (auto *edge : std::as_const(m_edgeItems)) {
        bool sourceDimmed = !edge->sourceNode()->nodeLabel().contains(text, Qt::CaseInsensitive);
        bool targetDimmed = !edge->targetNode()->nodeLabel().contains(text, Qt::CaseInsensitive);
        edge->setDimmed(sourceDimmed && targetDimmed);
    }
}
```

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

- [ ] **Step 5: Manual test**

Run `./build/Corbomite`, open a vault, open graph view (Ctrl+G). Verify:
1. Toggle button appears top-right. Click it to show panel.
2. Panel appears with three collapsed sections.
3. Expand Forces, drag sliders — graph re-layouts.
4. Expand Display, drag node size — nodes grow/shrink. Toggle arrows — arrowheads appear on edges.
5. Expand Filters, type in search — non-matching nodes dim. Toggle "Existing files only" — gray nodes disappear.
6. Click reset button — all sliders return to defaults.
7. Click X — panel hides, toggle button reappears.
8. Resize window — panel stays anchored top-right.

---

### Task 5: Right-Click Context Menu on Graph Nodes

**Files:**
- Modify: `libs/forcegraph/include/forcegraph/ForceGraphView.h`
- Modify: `libs/forcegraph/src/ForceGraphView.cpp`
- Modify: `src/graph/GraphViewTab.h`
- Modify: `src/graph/GraphViewTab.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

The forcegraph library emits a signal with the node ID and screen position. The app layer (GraphViewTab) builds the QMenu with KDE i18n strings and handles all actions. MainWindow gains new slots for cross-component actions (reveal in navigation, delete from graph).

- [ ] **Step 1: Add context menu signal to ForceGraphView**

In `libs/forcegraph/include/forcegraph/ForceGraphView.h`, add the signal and override:

```cpp
Q_SIGNALS:
    void nodeClicked(const QString &id);
    void nodeDoubleClicked(const QString &id);
    void nodeHovered(const QString &id);
    void nodeContextMenuRequested(const QString &id, const QPoint &globalPos);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
```

- [ ] **Step 2: Implement contextMenuEvent in ForceGraphView**

In `libs/forcegraph/src/ForceGraphView.cpp`, add the include and method:

```cpp
#include <QContextMenuEvent>
```

```cpp
void ForceGraphView::contextMenuEvent(QContextMenuEvent *event)
{
    auto *item = itemAt(event->pos());
    auto *nodeItem = dynamic_cast<ForceGraphNode *>(item);

    if (nodeItem) {
        Q_EMIT nodeContextMenuRequested(nodeItem->nodeId(), event->globalPos());
        event->accept();
        return;
    }

    QGraphicsView::contextMenuEvent(event);
}
```

- [ ] **Step 3: Add context menu signals and handler to GraphViewTab**

In `src/graph/GraphViewTab.h`, add new signals and a private method:

```cpp
Q_SIGNALS:
    void noteActivated(const QString &relativePath);
    void openInNewTabRequested(const QString &relativePath);
    void revealInNavigationRequested(const QString &relativePath);
    void deleteNoteRequested(const QString &relativePath);

private:
    // ... existing members ...
    void showNodeContextMenu(const QString &nodeId, const QPoint &globalPos);
```

- [ ] **Step 4: Implement showNodeContextMenu in GraphViewTab**

In `src/graph/GraphViewTab.cpp`, add the includes:

```cpp
#include "corbomite/models/VaultModel.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QUrl>
```

Connect the signal in the constructor (after `setupControlsPanel()`):

```cpp
    connect(m_graphView, &ForceGraph::ForceGraphView::nodeContextMenuRequested,
            this, &GraphViewTab::showNodeContextMenu);
```

Implement the method:

```cpp
void GraphViewTab::showNodeContextMenu(const QString &nodeId, const QPoint &globalPos)
{
    // Node label for the header
    QString label = nodeId;
    label = label.mid(label.lastIndexOf(QLatin1Char('/')) + 1);
    if (label.endsWith(QStringLiteral(".md"))) label.chop(3);

    QString vaultPath = m_vault->path();
    QString absolutePath = QDir(vaultPath).filePath(nodeId);

    QMenu menu(this);

    // Header — disabled, shows note name
    auto *headerAction = menu.addAction(label);
    headerAction->setEnabled(false);
    QFont boldFont = headerAction->font();
    boldFont.setBold(true);
    headerAction->setFont(boldFont);

    menu.addSeparator();

    // Open in new tab
    auto *openNewTab = menu.addAction(
        QIcon::fromTheme(QStringLiteral("tab-new")),
        i18n("Open in new tab"));

    // TODO: Open in new window (requires multi-window support)
    // auto *openNewWindow = menu.addAction(
    //     QIcon::fromTheme(QStringLiteral("window-new")),
    //     i18n("Open in new window"));

    menu.addSeparator();

    // TODO: Move file to... (requires file move dialog)
    // auto *moveFile = menu.addAction(
    //     QIcon::fromTheme(QStringLiteral("document-save-as")),
    //     i18n("Move file to..."));

    // TODO: Bookmark... (requires bookmark system)
    // auto *bookmark = menu.addAction(
    //     QIcon::fromTheme(QStringLiteral("bookmark-new")),
    //     i18n("Bookmark..."));

    // TODO: Merge entire file with... (requires merge UI)
    // auto *merge = menu.addAction(
    //     QIcon::fromTheme(QStringLiteral("merge")),
    //     i18n("Merge entire file with..."));

    // Copy path submenu
    auto *copyMenu = menu.addMenu(
        QIcon::fromTheme(QStringLiteral("edit-copy")),
        i18n("Copy path"));

    auto *copyVaultPath = copyMenu->addAction(i18n("Copy vault path"));
    auto *copyAbsPath = copyMenu->addAction(i18n("Copy absolute path"));

    // TODO: Open linked view (requires local graph as tab)
    // auto *linkedView = menu.addAction(
    //     QIcon::fromTheme(QStringLiteral("view-links")),
    //     i18n("Open linked view"));

    menu.addSeparator();

    auto *openDefault = menu.addAction(
        QIcon::fromTheme(QStringLiteral("document-open")),
        i18n("Open in default app"));

    auto *showInExplorer = menu.addAction(
        QIcon::fromTheme(QStringLiteral("system-file-manager")),
        i18n("Show in system explorer"));

    auto *revealInNav = menu.addAction(
        QIcon::fromTheme(QStringLiteral("view-list-tree")),
        i18n("Reveal file in navigation"));

    menu.addSeparator();

    auto *deleteFile = menu.addAction(
        QIcon::fromTheme(QStringLiteral("edit-delete")),
        i18n("Delete file"));

    // Execute menu
    auto *selected = menu.exec(globalPos);
    if (!selected) return;

    if (selected == openNewTab) {
        Q_EMIT openInNewTabRequested(nodeId);
    } else if (selected == copyVaultPath) {
        QApplication::clipboard()->setText(nodeId);
    } else if (selected == copyAbsPath) {
        QApplication::clipboard()->setText(absolutePath);
    } else if (selected == openDefault) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
    } else if (selected == showInExplorer) {
        QFileInfo fi(absolutePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    } else if (selected == revealInNav) {
        Q_EMIT revealInNavigationRequested(nodeId);
    } else if (selected == deleteFile) {
        Q_EMIT deleteNoteRequested(nodeId);
    }
}
```

- [ ] **Step 5: Wire new GraphViewTab signals in EditorViewSpace**

In `src/editor/EditorViewSpace.cpp`, where the GraphViewTab is created in `openGraphView()`, connect the new signals. After the existing `connect(graphTab, &GraphViewTab::noteActivated, ...)` line, add:

```cpp
    connect(graphTab, &GraphViewTab::openInNewTabRequested,
            this, &EditorViewSpace::graphNoteActivated);
    connect(graphTab, &GraphViewTab::revealInNavigationRequested,
            this, [this](const QString &path) {
        Q_EMIT revealInNavigationRequested(path);
    });
    connect(graphTab, &GraphViewTab::deleteNoteRequested,
            this, [this](const QString &path) {
        Q_EMIT deleteNoteFromGraphRequested(path);
    });
```

Add the new signals to `src/editor/EditorViewSpace.h`:

```cpp
Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void graphNoteActivated(const QString &relativePath);
    void revealInNavigationRequested(const QString &relativePath);
    void deleteNoteFromGraphRequested(const QString &relativePath);
```

- [ ] **Step 6: Forward signals through EditorViewManager**

In `src/editor/EditorViewManager.h`, add signals:

```cpp
Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void graphNoteActivated(const QString &relativePath);
    void revealInNavigationRequested(const QString &relativePath);
    void deleteNoteFromGraphRequested(const QString &relativePath);
```

In `src/editor/EditorViewManager.cpp`, in the `connectViewSpace()` method, add:

```cpp
    connect(space, &EditorViewSpace::revealInNavigationRequested,
            this, &EditorViewManager::revealInNavigationRequested);
    connect(space, &EditorViewSpace::deleteNoteFromGraphRequested,
            this, &EditorViewManager::deleteNoteFromGraphRequested);
```

- [ ] **Step 7: Handle signals in MainWindow**

In `src/app/MainWindow.h`, add `FileExplorerPanel` forward-declared method for revealing a file. Add to the private section:

```cpp
    void revealFileInNavigation(const QString &relativePath);
    void deleteNoteFromGraph(const QString &relativePath);
```

In `src/app/MainWindow.cpp`, in the `setupEditor()` method (where `m_editorManager` connections are set up), add:

```cpp
    connect(m_editorManager, &EditorViewManager::revealInNavigationRequested,
            this, &MainWindow::revealFileInNavigation);
    connect(m_editorManager, &EditorViewManager::deleteNoteFromGraphRequested,
            this, &MainWindow::deleteNoteFromGraph);
```

Implement the handlers:

```cpp
void MainWindow::revealFileInNavigation(const QString &relativePath)
{
    // FileExplorerPanel doesn't have a selectFile method yet.
    // For now, this is a forward-looking stub that will work once
    // FileExplorerPanel gains file selection capability.
    // TODO: Implement FileExplorerPanel::selectFile(relativePath)
    //       and call it here: m_fileExplorer->selectFile(relativePath);
    Q_UNUSED(relativePath);
}

void MainWindow::deleteNoteFromGraph(const QString &relativePath)
{
    auto result = KMessageBox::questionTwoActions(
        this,
        i18n("Delete \"%1\"?", relativePath),
        i18n("Delete Note"),
        KStandardGuiItem::del(),
        KStandardGuiItem::cancel()
    );
    if (result == KMessageBox::PrimaryAction) {
        m_vaultService->noteService()->deleteNote(relativePath);
    }
}
```

- [ ] **Step 8: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
```

- [ ] **Step 9: Manual test**

Run `./build/Corbomite`, open a vault, open graph view. Verify:
1. Right-click a node — menu appears with note name as header.
2. "Open in new tab" — note opens in editor.
3. "Copy path > Copy vault path" — vault-relative path on clipboard.
4. "Copy path > Copy absolute path" — full path on clipboard.
5. "Open in default app" — note opens in system default editor.
6. "Show in system explorer" — file manager opens to parent directory.
7. "Delete file" — confirmation dialog appears. Confirm — file is deleted.
8. Right-click empty space — no menu appears.

- [ ] **Step 10: Run tests**

```bash
cd build && ctest --output-on-failure
```

Verify no existing tests are broken by the changes.
