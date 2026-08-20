// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesView.h"

#include "corbomite/bases/BasesCellDelegate.h"
#include "corbomite/bases/BasesCommands.h"
#include "corbomite/bases/BasesHeaderView.h"
#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/QueryController.h"
#include "corbomite/bases/SortCycle.h"
#include "corbomite/bases/PropertiesMenuPanel.h"
#include "corbomite/bases/SortGroupMenuPanel.h"
#include "corbomite/bases/ViewsMenuPanel.h"
#include "corbomite/bases/FilterBuilderDialog.h"
#include "corbomite/bases/FilterSpec.h"
#include "corbomite/bases/FormulaEditDialog.h"
#include "corbomite/bases/FormulaCandidates.h"
#include "corbomite/bases/FormulaOps.h"

#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesVaultResolver.h"
#include "corbomite/bases/PropertiesDrawer.h"
#include "corbomite/bases/BasesQueryResult.h"
#include "corbomite/bases/TableExporter.h"
#include "corbomite/bases/Values.h"

#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QSaveFile>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

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

    m_filtersBtn = new QToolButton(this);
    m_filtersBtn->setText(i18n("Filters"));
    m_filtersBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_filtersBtn, &QToolButton::clicked, this, &BasesView::openFiltersDialog);
    toolbar->addWidget(m_filtersBtn);

    m_drawerBtn = new QToolButton(this);
    m_drawerBtn->setText(i18n("Properties pane"));
    m_drawerBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_drawerBtn->setCheckable(true);
    toolbar->addWidget(m_drawerBtn);

    m_newBtn = new QToolButton(this);
    m_newBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_newBtn->setToolTip(i18n("New entry"));
    connect(m_newBtn, &QToolButton::clicked, this, &BasesView::onNewItem);
    toolbar->addWidget(m_newBtn);

    m_resultsBtn = new QToolButton(this);
    m_resultsBtn->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    m_resultsBtn->setToolTip(i18n("Export / copy table"));
    m_resultsBtn->setPopupMode(QToolButton::InstantPopup);
    {
        auto *menu = new QMenu(m_resultsBtn);
        menu->addAction(i18n("Copy table"), this, &BasesView::onCopyTable);
        menu->addAction(i18n("Export CSV…"), this, &BasesView::onExportCsv);
        m_resultsBtn->setMenu(menu);
    }
    toolbar->addWidget(m_resultsBtn);

    m_propsPanel = new PropertiesMenuPanel(this);
    m_sortPanel  = new SortGroupMenuPanel(this);
    m_viewsPanel = new ViewsMenuPanel(this);
    m_propsPanel->setOnChanged([this]() { onConfigMutated(); });
    m_sortPanel->setOnChanged([this]()  { onConfigMutated(); });

    connect(m_propsPanel, &PropertiesMenuPanel::addFormulaRequested, this,
            [this]() { openFormulaDialog(QString()); });
    connect(m_propsPanel, &PropertiesMenuPanel::editFormulaRequested, this,
            [this](const QString &name) { openFormulaDialog(name); });
    connect(m_propsPanel, &PropertiesMenuPanel::deleteFormulaRequested, this,
            [this](const QString &name) {
                if (!m_query) return;
                FormulaOps::remove(m_query->formulas, m_query->formulaOrder, name);
                onConfigMutated();
            });
    connect(m_propsPanel, &PropertiesMenuPanel::summaryChanged, this,
            [this](const PropertyId &p, const QString &fn) {
                if (fn == QString::fromLatin1(kCustomSummarySentinel)) openSummaryDialog(p);
                else applySummaryChoice(p, fn);
            });
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
        // Feed summary state BEFORE setState so rebuild() finds m_summaryNames populated.
        m_propsPanel->setSummaryState(summaryNamesForPicker(),
            [this](const PropertyId &p) {
                return m_activeView ? m_activeView->summaries.value(p) : QString();
            });
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

    connect(m_delegate, &BasesCellDelegate::linkClicked,
            this, &BasesView::onLinkClicked);
    connect(m_delegate, &BasesCellDelegate::tagClicked, this, [this](const QString &tag) {
        if (m_searchTag) m_searchTag(tag);
    });
    connect(m_delegate, &BasesCellDelegate::urlClicked, this, [](const QString &url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    m_table->setDragEnabled(true);
    m_table->setDragDropMode(QAbstractItemView::DragOnly);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested,
            this, &BasesView::onContextMenu);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_table);
    m_drawer = new PropertiesDrawer(m_splitter);
    connect(m_drawer, &PropertiesDrawer::frontMatterEditRequested,
            this, &BasesView::pushFrontMatterEdit);
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

void BasesView::pushFrontMatterEdit(Corbomite::TFile *file, const QString &key,
                                    const QVariant &value)
{
    if (!m_fm || !file) return;
    auto notify = [this](const QString &msg) {
        m_errorBanner->setText(msg);
        m_errorBanner->show();
        QTimer::singleShot(4000, this, [this]() {
            if (m_errorBanner) m_errorBanner->hide();
        });
    };
    m_undoStack.push(new CmdSetFrontMatter(m_fm, file, key, value, notify));
}

void BasesView::undo() { m_undoStack.undo(); }
void BasesView::redo() { m_undoStack.redo(); }

void BasesView::focusSearch()
{
    if (!m_searchEdit) return;
    m_searchEdit->setFocus(Qt::ShortcutFocusReason);
    m_searchEdit->selectAll();
}

void BasesView::loadBaseFromVault()
{
    m_undoStack.clear();   // a history never spans two base loads
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
    connect(m_model.get(), &BasesTreeModel::frontMatterEditRequested,
            this, &BasesView::pushFrontMatterEdit, Qt::UniqueConnection);
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

QStringList BasesView::summaryNamesForPicker() const
{
    QStringList names{
        QStringLiteral("average"), QStringLiteral("sum"), QStringLiteral("min"),
        QStringLiteral("max"), QStringLiteral("median"), QStringLiteral("stddev"),
        QStringLiteral("unique"), QStringLiteral("count") };
    if (m_query)
        for (const auto &n : m_query->summaryFormulaOrder)
            if (!names.contains(n)) names << n;
    return names;
}

QStringList BasesView::formulaCandidateList() const
{
    return FormulaCandidates::build(availableProperties(),
                                    m_funcs ? m_funcs : &FunctionRegistry::global(),
                                    FormulaCandidates::Mode::NamedFormula);
}

void BasesView::openFormulaDialog(const QString &editName)
{
    if (!m_query) return;
    FormulaEditDialog dlg(FormulaCandidates::Mode::NamedFormula, this);
    dlg.setCandidates(formulaCandidateList());
    QStringList existing = m_query->formulaOrder;
    existing.removeAll(editName);                 // editing its own name is fine
    dlg.setExistingNames(existing);
    if (!editName.isEmpty())
        dlg.setInitial(editName, m_query->formulas.value(editName).source());
    if (dlg.exec() != QDialog::Accepted) return;

    const QString name = dlg.formulaName();
    const QString src  = dlg.formulaSource();
    if (editName.isEmpty()) {
        FormulaOps::add(m_query->formulas, m_query->formulaOrder, name, src);
    } else if (name != editName) {
        FormulaOps::rename(m_query->formulas, m_query->formulaOrder, editName, name);
        FormulaOps::setSource(m_query->formulas, name, src);
    } else {
        FormulaOps::setSource(m_query->formulas, name, src);
    }
    onConfigMutated();
}

void BasesView::openSummaryDialog(const PropertyId &prop)
{
    if (!m_query) return;
    FormulaEditDialog dlg(FormulaCandidates::Mode::SummaryFormula, this);
    dlg.setCandidates(FormulaCandidates::build(
        availableProperties(),
        m_funcs ? m_funcs : &FunctionRegistry::global(),
        FormulaCandidates::Mode::SummaryFormula));
    dlg.setExistingNames(m_query->summaryFormulaOrder);
    if (dlg.exec() != QDialog::Accepted) return;
    FormulaOps::add(m_query->summaryFormulas, m_query->summaryFormulaOrder,
                    dlg.formulaName(), dlg.formulaSource());
    applySummaryChoice(prop, dlg.formulaName());
}

void BasesView::applySummaryChoice(const PropertyId &prop, const QString &fnName)
{
    if (!m_activeView) return;
    if (fnName.isEmpty()) m_activeView->summaries.remove(prop);
    else m_activeView->summaries.insert(prop, fnName);
    onConfigMutated();
}

void BasesView::openFiltersDialog()
{
    if (!m_query || !m_activeView) return;
    FilterBuilderDialog dlg(this);
    dlg.setScopes(fromFilter(m_query->filters),
                  fromFilter(m_activeView->filters),
                  formulaCandidateList());
    if (dlg.exec() != QDialog::Accepted) return;
    applyFilterSpecs(dlg.globalSpec(), dlg.perViewSpec());
}

void BasesView::applyFilterSpecs(const FilterSpec &globalSpec, const FilterSpec &perViewSpec)
{
    if (!m_query || !m_activeView) return;
    m_query->filters = toFilter(globalSpec);
    m_activeView->filters = toFilter(perViewSpec);
    onConfigMutated();
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

QString BasesView::resolveLink(const QString &target) const
{
    if (!m_vault) return {};
    BasesVaultResolver resolver(m_vault, m_cache);
    const QString src = m_query ? m_query->filePath : QString{};
    return resolver.resolveLinkTarget(target, src);   // "" if unresolved
}

void BasesView::onLinkClicked(const QString &target, Qt::KeyboardModifiers mods)
{
    const QString path = resolveLink(target);
    if (path.isEmpty()) return;                       // unresolved -> no-op
    const bool newTab = mods.testFlag(Qt::ControlModifier)
                     || mods.testFlag(Qt::MetaModifier);
    if (newTab) {
        if (m_openInNewTab) m_openInNewTab(path);
        return;
    }
    // Same-tab navigation: drive the base's own leaf (history-aware).
    if (auto *lf = leaf()) {
        QJsonObject state;
        state[QStringLiteral("type")] = QStringLiteral("markdown");
        state[QStringLiteral("state")] = QJsonObject{{QStringLiteral("file"), path}};
        lf->navigate(state);
    } else if (m_openInNewTab) {
        m_openInNewTab(path);                         // fallback
    }
}

void BasesView::onContextMenu(const QPoint &pos)
{
    if (!m_model) return;
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid() || m_model->isGroupRow(idx)) return;

    QMenu menu(this);
    // Resolve the row's note (entry's own file) for file actions.
    QString notePath;
    if (BasesEntry *e = m_model->entryForIndex(idx); e && e->file())
        notePath = e->file()->path;

    // If the clicked cell is a wikilink, prefer its target.
    const QString type = idx.data(BasesTreeModel::ValueTypeRole).toString();
    if (type == QLatin1String("Link")) {
        const auto v = idx.data(BasesTreeModel::ValuePtrRole).value<ValuePtr>();
        if (auto *s = dynamic_cast<StringValue *>(v.get())) {
            const QString resolved = resolveLink(s->data());
            if (!resolved.isEmpty()) notePath = resolved;
        }
    }

    if (!notePath.isEmpty()) {
        const QString path = notePath;
        menu.addAction(i18n("Open"), this, [this, path]() {
            if (auto *lf = leaf()) {
                QJsonObject st; st[QStringLiteral("type")] = QStringLiteral("markdown");
                st[QStringLiteral("state")] = QJsonObject{{QStringLiteral("file"), path}};
                lf->navigate(st);
            }
        });
        menu.addAction(i18n("Open in new tab"), this, [this, path]() {
            if (m_openInNewTab) m_openInNewTab(path);
        });
        menu.addAction(i18n("Copy as wikilink"), this, [path]() {
            const QString base = QFileInfo(path).completeBaseName();
            QApplication::clipboard()->setText(QStringLiteral("[[%1]]").arg(base));
        });
        menu.addSeparator();
        menu.addAction(i18n("Rename…"), this, [this, path]() {
            if (m_promptRename) m_promptRename(path);
        });
        menu.addAction(i18n("Delete"), this, [this, path]() {
            if (m_promptDelete) m_promptDelete(path);
        });
    } else {
        const QString display = idx.data(Qt::DisplayRole).toString();
        menu.addAction(i18n("Copy value"), this, [display]() {
            QApplication::clipboard()->setText(display);
        });
    }
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void BasesView::onCopyTable()
{
    if (!m_controller || !m_controller->result()) return;
    TableExporter exp(*m_controller->result(),
                      [this](const PropertyId &pid) { return displayNameFor(pid); });

    auto *mime = new QMimeData();
    mime->setText(exp.toTsv());  // text/plain = TSV (spreadsheet-friendly default).
    mime->setData(QStringLiteral("text/markdown"), exp.toMarkdown().toUtf8());
    mime->setHtml(exp.toHtml());
    mime->setData(QStringLiteral("obsidian/table"), exp.toObsidianTable());
    QApplication::clipboard()->setMimeData(mime);
}

void BasesView::onExportCsv()
{
    if (!m_controller || !m_controller->result()) return;

    QString suggested = QStringLiteral("table.csv");
    if (m_query && !m_query->filePath.isEmpty()) {
        const QString stem = QFileInfo(m_query->filePath).completeBaseName();
        if (!stem.isEmpty()) suggested = stem + QStringLiteral(".csv");
    }
    const QString path = QFileDialog::getSaveFileName(
        this, i18n("Export table as CSV"), suggested,
        i18n("CSV files (*.csv);;All files (*)"));
    if (path.isEmpty()) return;

    TableExporter exp(*m_controller->result(),
                      [this](const PropertyId &pid) { return displayNameFor(pid); });
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(exp.toCsv().toUtf8()) < 0 || !f.commit()) {
        m_errorBanner->setText(i18n("Failed to write CSV: %1", path));
        m_errorBanner->show();
    }
}

NewItemSeed::SeedList BasesView::resolveTemplateProps() const
{
    NewItemSeed::SeedList out;
    if (!m_query || !m_query->newItemTemplate.has_value() || !m_cache) return out;
    const QString tmplPath = m_query->newItemTemplate.value();
    if (tmplPath.isEmpty()) return out;
    const auto cache = m_cache->getFileCache(tmplPath);
    if (!cache || !cache->frontmatter.has_value()) return out;
    const QJsonObject fm = cache->frontmatter.value();
    for (auto it = fm.constBegin(); it != fm.constEnd(); ++it) {
        // The string-based seed pipeline can only carry scalars. Stringifying
        // an array/object value here would write lossy garbage (e.g. a list
        // collapses to an empty string), so skip non-scalar template keys
        // rather than corrupt the new note's frontmatter.
        const QJsonValue v = it.value();
        if (v.isArray() || v.isObject()) continue;
        out.append({it.key(), v.toVariant().toString()});
    }
    return out;
}

void BasesView::onNewItem()
{
    if (!m_fm) return;

    QString folder;
    if (m_query && m_query->newItemFolder.has_value())
        folder = m_query->newItemFolder.value();

    BasesViewConfig *view = m_activeView;
    FilterPtr filter = view ? view->filters : FilterPtr{};
    if (!filter && m_query) filter = m_query->filters;
    const NewItemSeed::SeedList seed = NewItemSeed::compute(filter, resolveTemplateProps());

    Corbomite::TFile *file = m_fm->createMarkdownNote(QStringLiteral("Untitled"), folder);
    if (!file) {
        m_errorBanner->setText(i18n("Failed to create new note"));
        m_errorBanner->show();
        return;
    }

    if (!seed.isEmpty()) {
        m_fm->processFrontMatter(file, [&seed](QVariantMap &fm) {
            for (const auto &p : seed) fm.insert(p.first, p.second);
        });
    }

    const QString path = file->path;
    if (m_openInNewTab) m_openInNewTab(path);
    if (m_promptRename) m_promptRename(path);
}

}  // namespace Corbomite::Bases
