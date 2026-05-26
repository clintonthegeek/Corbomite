// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesView.h"

#include "corbomite/bases/BasesCellDelegate.h"
#include "corbomite/bases/BasesHeaderView.h"
#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/QueryController.h"
#include "corbomite/bases/SortCycle.h"
#include "corbomite/bases/PropertiesMenuPanel.h"
#include "corbomite/bases/SortGroupMenuPanel.h"
#include "corbomite/bases/ViewsMenuPanel.h"

#include "corbomite/core/NoteDocument.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/bases/PropertiesDrawer.h"
#include "corbomite/bases/BasesQueryResult.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace Corbomite::Bases {

BasesView::BasesView(WorkspaceLeaf *leaf, QWidget *parent)
    : TextFileView(leaf, parent)
{
    // Build into the ItemView-provided content area, NOT `this` — ItemView's
    // ctor already installs an outer layout (header chrome + contentWidget).
    // Creating a second layout on `this` is rejected by Qt ("already has a
    // layout") and leaves our toolbar/table unmanaged + invisible.
    auto *root = new QVBoxLayout(contentWidget());
    root->setContentsMargins(0, 0, 0, 0);

    // Toolbar row: view switcher + search field.
    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(6, 4, 6, 4);

    m_viewSelector = new QComboBox(this);
    toolbar->addWidget(m_viewSelector);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(i18n("Search"));
    m_searchEdit->setClearButtonEnabled(true);
    toolbar->addWidget(m_searchEdit);

    m_propsBtn = new QToolButton(this);
    m_propsBtn->setText(i18n("Properties"));
    m_propsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addWidget(m_propsBtn);

    m_sortBtn = new QToolButton(this);
    m_sortBtn->setText(i18n("Sort & group"));
    m_sortBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addWidget(m_sortBtn);

    m_viewsBtn = new QToolButton(this);
    m_viewsBtn->setText(i18n("Views"));
    m_viewsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addWidget(m_viewsBtn);

    m_drawerBtn = new QToolButton(this);
    m_drawerBtn->setText(i18n("Properties pane"));
    m_drawerBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_drawerBtn->setCheckable(true);
    toolbar->addWidget(m_drawerBtn);

    m_propsPanel = new PropertiesMenuPanel(this);
    m_sortPanel  = new SortGroupMenuPanel(this);
    m_viewsPanel = new ViewsMenuPanel(this);
    m_propsPanel->setOnChanged([this]() { onConfigMutated(); });
    m_sortPanel->setOnChanged([this]()  { onConfigMutated(); });
    m_viewsPanel->setOnChanged([this]() {
        populateViewSelector();
        requestSave();
    });
    m_viewsPanel->setOnActivate([this](const QString &name) {
        m_viewSelector->setCurrentText(name);   // triggers onViewSelectorChanged
    });

    // Watch the popup panels for hide events so a click on the same button
    // right after its panel dismissed (outside-click) is debounced below.
    m_propsPanel->installEventFilter(this);
    m_sortPanel->installEventFilter(this);
    m_viewsPanel->installEventFilter(this);

    connect(m_propsBtn, &QToolButton::clicked, this, [this]() {
        if (m_panelDismissTimer.isValid() && m_panelDismissTimer.elapsed() < 150) return;
        if (!m_activeView) return;
        m_propsPanel->setState(&m_activeView->order, availableProperties(),
                               [this](const PropertyId &p) { return displayNameFor(p); });
        showPanelUnder(m_propsPanel, m_propsBtn);
    });
    connect(m_sortBtn, &QToolButton::clicked, this, [this]() {
        if (m_panelDismissTimer.isValid() && m_panelDismissTimer.elapsed() < 150) return;
        if (!m_activeView) return;
        m_sortPanel->setState(m_activeView, availableProperties(),
                              [this](const PropertyId &p) { return displayNameFor(p); });
        showPanelUnder(m_sortPanel, m_sortBtn);
    });
    connect(m_viewsBtn, &QToolButton::clicked, this, [this]() {
        if (m_panelDismissTimer.isValid() && m_panelDismissTimer.elapsed() < 150) return;
        if (!m_query) return;
        m_viewsPanel->setState(m_query.get(), m_activeView ? m_activeView->name : QString{});
        showPanelUnder(m_viewsPanel, m_viewsBtn);
    });

    root->addLayout(toolbar);

    m_errorBanner = new QLabel(this);
    m_errorBanner->setStyleSheet(QStringLiteral("QLabel { color: #a33; padding: 4px; }"));
    m_errorBanner->hide();
    root->addWidget(m_errorBanner);

    m_table = new QTreeView(this);
    auto *hdr = new BasesHeaderView(m_table);
    m_table->setHeader(hdr);
    hdr->setProviders(
        [this]() { return m_activeView ? m_activeView->sort : QVector<SortKey>{}; },
        [this](int c) { return m_model ? m_model->propertyAt(c) : PropertyId{}; });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setRootIsDecorated(true);
    m_table->setExpandsOnDoubleClick(true);
    m_table->setItemsExpandable(true);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::SelectedClicked
                           | QAbstractItemView::EditKeyPressed);
    m_delegate = new BasesCellDelegate(this);
    m_table->setItemDelegate(m_delegate);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_table);
    m_drawer = new PropertiesDrawer(m_splitter);
    m_drawer->hide();                         // collapsed until toggled
    m_splitter->addWidget(m_drawer);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    root->addWidget(m_splitter, 1);

    connect(m_drawerBtn, &QToolButton::toggled, this, [this](bool on) {
        m_drawer->setVisible(on);
        if (on) onSelectionChanged();
    });

    connect(hdr, &QHeaderView::sectionClicked,
            this, &BasesView::onHeaderClicked);
    connect(hdr, &QHeaderView::sectionMoved,
            this, &BasesView::onSectionMoved);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &BasesView::onSearchChanged);
    connect(m_viewSelector, &QComboBox::currentTextChanged,
            this, &BasesView::onViewSelectorChanged);
}

BasesView::~BasesView() = default;

Corbomite::View *BasesView::factory(WorkspaceLeaf *leaf)
{
    return new BasesView(leaf);
}

void BasesView::setServices(Vault *vault, MetadataCache *cache,
                            FileManager *fileManager, FunctionRegistry *funcs)
{
    m_vault = vault;
    m_cache = cache;
    m_fm = fileManager;
    if (m_drawer) m_drawer->setFileManager(m_fm);
    m_funcs = funcs ? funcs : &FunctionRegistry::global();

    // Services and file-content arrive in either order in the host
    // (propagateServicesToView vs onLoadFile). Whichever completes the
    // vault+file pair last triggers the actual load + parse.
    loadBaseFromVault();
    // Recovery: if the query was supplied directly (setViewData) before the
    // vault landed, rebuildLayout() bailed earlier — build it now.
    if (m_query && !m_model && m_vault)
        rebuildLayout();
}

void BasesView::loadBaseFromVault()
{
    // Need both the vault (for a raw read) and the loaded document. Skip if a
    // model is already built (idempotent across repeated setServices calls).
    if (!m_vault || !file() || m_model)
        return;
    TFile *tf = m_vault->getFileByPath(file()->relativePath());
    if (!tf)
        return;
    // Raw bytes — NOT NoteDocument::markdown(), which would route the .base
    // YAML through the Markdown parser and corrupt it.
    setViewData(QString::fromUtf8(m_vault->read(tf)), true);
}

void BasesView::onLoadFile(Corbomite::NoteDocument *file)
{
    // Run base wiring (rename/delete subscriptions). TextFileView's adapter
    // read is inert (no DataAdapter injected), so it won't load content.
    TextFileView::onLoadFile(file);
    loadBaseFromVault();
}

QString BasesView::getViewData() const
{
    return m_query ? m_query->toString() : QString{};
}

void BasesView::setViewData(const QString &data, bool clear)
{
    Q_UNUSED(clear);
    QString err;
    auto q = BasesQuery::fromString(data, &err);
    if (!err.isEmpty()) {
        m_errorBanner->setText(i18n("Base parse error: %1", err));
        m_errorBanner->show();
    } else {
        m_errorBanner->hide();
    }
    m_query = std::move(q);
    m_activeView = m_query->getViewConfig();
    populateViewSelector();
    rebuildLayout();
}

void BasesView::clear()
{
    m_query.reset();
    m_activeView = nullptr;
    m_table->setModel(nullptr);
    m_model.reset();
    m_controller.reset();
}

void BasesView::setActiveView(const QString &name)
{
    if (!m_query) return;
    m_activeView = m_query->getViewConfig(name);
    if (m_controller) m_controller->setViewConfig(m_activeView);
}

void BasesView::setCurrentFile(Corbomite::TFile *file)
{
    if (m_controller) m_controller->setCurrentFile(file);
}

void BasesView::populateViewSelector()
{
    QSignalBlocker _(m_viewSelector);
    m_viewSelector->clear();
    if (!m_query) return;
    for (const auto &v : m_query->views) {
        if (v) m_viewSelector->addItem(v->name);
    }
    if (m_activeView) m_viewSelector->setCurrentText(m_activeView->name);
}

void BasesView::rebuildLayout()
{
    if (!m_vault || !m_query) return;

    m_controller = std::make_unique<QueryController>(
        m_vault, m_cache, m_funcs, this);
    m_controller->setQuery(m_query);
    m_controller->setViewConfig(m_activeView);

    m_model = std::make_unique<BasesTreeModel>(m_controller.get(), m_fm, this);
    m_table->setModel(m_model.get());
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &BasesView::onSelectionChanged, Qt::UniqueConnection);
    m_table->expandAll();
    m_controller->recomputeNow();
}

void BasesView::onHeaderClicked(int column)
{
    if (!m_activeView || !m_model) return;
    const PropertyId pid = m_model->propertyAt(column);
    if (pid.name.isEmpty()) return;
    const bool shift = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    cycleHeaderSort(m_activeView->sort, pid, shift);
    if (m_controller) m_controller->recomputeNow();
    if (m_table) {
        m_table->header()->update();     // repaint sort indicators
        m_table->expandAll();            // keep groups visible after re-sort
    }
    requestSave();                       // persist, as reorder/view-switch already do
}

void BasesView::onSearchChanged(const QString &text)
{
    if (m_controller) m_controller->setSearchQuery(text);
}

void BasesView::onViewSelectorChanged(const QString &name)
{
    setActiveView(name);
    requestSave();
}

void BasesView::onSectionMoved(int, int, int)
{
    if (!m_activeView || !m_model) return;
    auto *header = m_table->header();
    const int columnCount = m_model->columnCount();
    QVector<PropertyId> newOrder;
    newOrder.reserve(columnCount);
    for (int visual = 0; visual < columnCount; ++visual) {
        const int logical = header->logicalIndex(visual);
        if (logical < 0 || logical >= columnCount) continue;
        newOrder.push_back(m_model->propertyAt(logical));
    }
    if (newOrder == m_activeView->order) return;
    m_activeView->order = std::move(newOrder);
    requestSave();
}

void BasesView::onSelectionChanged()
{
    if (!m_drawer || !m_drawer->isVisible() || !m_model) return;
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid() || m_model->isGroupRow(idx)) { m_drawer->showEntry(nullptr); return; }
    m_drawer->showEntry(m_model->entryForIndex(idx));
}

void BasesView::onConfigMutated()
{
    if (m_controller) m_controller->recomputeNow();
    if (m_table) m_table->expandAll();
    requestSave();
}

QVector<PropertyId> BasesView::availableProperties() const
{
    if (m_controller && m_controller->result())
        return m_controller->result()->properties();
    return m_activeView ? m_activeView->order : QVector<PropertyId>{};
}

QString BasesView::displayNameFor(const PropertyId &pid) const
{
    if (m_query) {
        auto it = m_query->properties.constFind(pid);
        if (it != m_query->properties.constEnd() && !it->displayName.isEmpty())
            return it->displayName;
    }
    return pid.name;
}

bool BasesView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Hide
        && (watched == m_propsPanel || watched == m_sortPanel || watched == m_viewsPanel)) {
        m_panelDismissTimer.restart();
    }
    return TextFileView::eventFilter(watched, event);
}

void BasesView::showPanelUnder(QWidget *panel, QToolButton *button)
{
    const QPoint below = button->mapToGlobal(QPoint(0, button->height()));
    panel->move(below);
    panel->show();
    panel->raise();
}

}  // namespace Corbomite::Bases
