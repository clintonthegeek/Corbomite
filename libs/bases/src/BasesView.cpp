// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesView.h"

#include "corbomite/bases/BasesCellDelegate.h"
#include "corbomite/bases/BasesTableModel.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/QueryController.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QVBoxLayout>

namespace Corbomite::Bases {

BasesView::BasesView(WorkspaceLeaf *leaf, QWidget *parent)
    : TextFileView(leaf, parent)
{
    auto *root = new QVBoxLayout(this);
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

    root->addLayout(toolbar);

    m_errorBanner = new QLabel(this);
    m_errorBanner->setStyleSheet(QStringLiteral("QLabel { color: #a33; padding: 4px; }"));
    m_errorBanner->hide();
    root->addWidget(m_errorBanner);

    m_table = new QTableView(this);
    m_table->horizontalHeader()->setSectionsClickable(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::SelectedClicked
                           | QAbstractItemView::EditKeyPressed);
    m_delegate = new BasesCellDelegate(this);
    m_table->setItemDelegate(m_delegate);
    root->addWidget(m_table, 1);

    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &BasesView::onHeaderClicked);
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
    m_funcs = funcs ? funcs : &FunctionRegistry::global();
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

    m_model = std::make_unique<BasesTableModel>(m_controller.get(), m_fm, this);
    m_table->setModel(m_model.get());
    m_controller->recomputeNow();
}

void BasesView::onHeaderClicked(int column)
{
    if (!m_activeView || !m_model) return;
    const PropertyId pid = m_model->propertyAt(column);
    // Cycle: ascending -> descending -> unsorted.
    QString newDir;
    if (!m_activeView->sort.isEmpty() && m_activeView->sort.front().property == pid) {
        newDir = m_activeView->sort.front().direction == QLatin1String("ASC")
                     ? QStringLiteral("DESC") : QString{};
    } else {
        newDir = QStringLiteral("ASC");
    }
    m_activeView->sort.clear();
    if (!newDir.isEmpty())
        m_activeView->sort.push_back({pid, newDir});
    if (m_controller) m_controller->recomputeNow();
    requestSave();
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

}  // namespace Corbomite::Bases
