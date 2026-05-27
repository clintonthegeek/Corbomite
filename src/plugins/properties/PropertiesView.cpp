// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesView.h"

#include "../../sidebar/PropertyEditorWidget.h"
#include "../../sidebar/PropertyRow.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/models/PropertyType.h"
#include "corbomite/models/PropertyTypeInference.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>

#include <QDebug>
#include <QFont>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace Corbomite {

PropertiesView::PropertiesView(MetadataCacheReader *metadata,
                                VaultProxy *vault,
                                FileManagerProxy *fileManager,
                                WorkspaceController *workspace,
                                QWidget *parent)
    : QWidget(parent)
    , m_metadata(metadata)
    , m_vaultProxy(vault)
    , m_fmProxy(fileManager)
    , m_workspace(workspace)
    , m_headerLabel(new QLabel(i18n("Properties"), this))
    , m_emptyLabel(new QLabel(i18n("No properties"), this))
    , m_addPropertyButton(new QPushButton(i18n("+ Add property"), this))
    , m_writeDebounce(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    layout->addWidget(m_headerLabel);

    m_rowsContainer = new QWidget(this);
    m_rowsLayout = new QVBoxLayout(m_rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    layout->addWidget(m_rowsContainer);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_emptyLabel);

    layout->addWidget(m_addPropertyButton);
    layout->addStretch(1);

    m_writeDebounce->setSingleShot(true);
    m_writeDebounce->setInterval(500);
    connect(m_writeDebounce, &QTimer::timeout, this, &PropertiesView::flushWrite);

    connect(m_addPropertyButton, &QPushButton::clicked, this,
            &PropertiesView::onAddPropertyClicked);

    if (m_metadata) {
        connect(m_metadata, &MetadataCacheReader::cacheChanged, this,
                &PropertiesView::onCacheChanged);
    }
    if (m_workspace) {
        connect(m_workspace, &WorkspaceController::activeFileChanged, this,
                &PropertiesView::onActiveFileChanged);
        m_currentPath = m_workspace->activeFilePath();
    }
    refresh();
}

PropertiesView::~PropertiesView() = default;

void PropertiesView::onActiveFileChanged(const QString &path)
{
    if (m_currentPath == path) return;
    m_writeDebounce->stop();
    m_currentPath = path;
    refresh();
}

void PropertiesView::onCacheChanged(const QString &path)
{
    if (path != m_currentPath) return;
    if (m_writeDebounce->isActive()) return; // suppress while user edit pending
    refresh();
}

void PropertiesView::onAddPropertyClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("Add property"),
        i18n("Property name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    addProperty(name.trimmed(), PropertyType::Text);
}

// ---- Interaction API ----

void PropertiesView::addProperty(const QString &name, PropertyType type)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || m_currentPath.isEmpty() || keyExists(n)) return;
    appendRow(n, type, Markoff::YamlValue(), /*editable=*/true);
    m_emptyLabel->setVisible(false);
    m_rowsContainer->setVisible(true);
    scheduleWrite();
}

bool PropertiesView::renameProperty(const QString &, const QString &)
{
    return false; // Task 6
}

void PropertiesView::deleteProperty(const QString &key)
{
    const int i = indexOfKey(key);
    if (i < 0) return;
    PropertyRow *row = m_rows.takeAt(i);
    m_rowsLayout->removeWidget(row);
    row->deleteLater();
    if (m_rows.isEmpty()) { m_emptyLabel->setVisible(true); m_rowsContainer->setVisible(false); }
    scheduleWrite();
}

void PropertiesView::moveProperty(int, int)
{
    // Task 7
}

// ---- Row management ----

void PropertiesView::clearRows()
{
    for (PropertyRow *r : m_rows) { m_rowsLayout->removeWidget(r); r->deleteLater(); }
    m_rows.clear();
}

void PropertiesView::appendRow(const QString &key, PropertyType type,
                               const Markoff::YamlValue &value, bool editable)
{
    auto *row = new PropertyRow(key, type, value, editable, m_rowsContainer);
    connect(row, &PropertyRow::valueChanged, this, &PropertiesView::scheduleWrite);
    connect(row, &PropertyRow::deleteRequested, this,
            [this, row]() { deleteProperty(row->key()); });
    connect(row, &PropertyRow::keyRenameRequested, this,
            [this](const QString &o, const QString &n) { renameProperty(o, n); });
    m_rowsLayout->addWidget(row);
    m_rows.push_back(row);
}

void PropertiesView::refresh()
{
    clearRows();

    if (m_currentPath.isEmpty() || !m_metadata) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_rowsContainer->setVisible(false);
        m_addPropertyButton->setEnabled(false);
        return;
    }
    m_addPropertyButton->setEnabled(true);
    const QJsonObject fm = m_metadata->frontmatterFor(m_currentPath);
    if (fm.isEmpty()) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_rowsContainer->setVisible(false);
        return;
    }
    m_emptyLabel->setVisible(false);
    m_rowsContainer->setVisible(true);
    m_headerLabel->setText(i18n("Properties (%1)", fm.size()));
    rebuildFromFrontmatter(fm);
}

void PropertiesView::rebuildFromFrontmatter(const QJsonObject &fm)
{
    for (auto it = fm.begin(); it != fm.end(); ++it) {
        const Markoff::YamlValue yval = qJsonValueToYaml(it.value());
        const bool editable = isEditableFrontmatterValue(yval);
        const PropertyType type = editable ? inferPropertyType(yval) : PropertyType::Text;
        appendRow(it.key(), type, yval, editable);
    }
}

int PropertiesView::rowCount() const { return m_rows.size(); }

void PropertiesView::flushPendingWrite()
{
    if (m_writeDebounce->isActive()) {
        m_writeDebounce->stop();
        flushWrite();
    }
}

void PropertiesView::scheduleWrite() { m_writeDebounce->start(); }

void PropertiesView::flushWrite()
{
    if (m_currentPath.isEmpty() || !m_vaultProxy || !m_fmProxy) return;
    auto *tf = m_vaultProxy->getFileByPath(m_currentPath);
    if (!tf) return;

    QList<FileManager::FrontMatterEntry> entries;
    entries.reserve(m_rows.size());
    for (PropertyRow *row : m_rows) {
        if (row->preserveFromDisk()) {
            entries.push_back({row->key(), QVariant{}, true});
            continue;
        }
        const Markoff::YamlValue v = row->currentValue();
        QVariant qv;
        switch (v.kind()) {
        case Markoff::YamlValue::Kind::Bool:   qv = v.asBool(); break;
        case Markoff::YamlValue::Kind::Int:    qv = QVariant::fromValue<qlonglong>(v.asInt()); break;
        case Markoff::YamlValue::Kind::Double: qv = v.asDouble(); break;
        case Markoff::YamlValue::Kind::String: qv = v.asString(); break;
        case Markoff::YamlValue::Kind::Seq:    qv = v.asStringList(); break;
        case Markoff::YamlValue::Kind::Null:
        default:                               qv = QVariant(); break;
        }
        entries.push_back({row->key(), qv, false});
    }
    if (!m_fmProxy->setFrontMatter(tf, entries))
        qWarning() << "PropertiesView: setFrontMatter failed for" << m_currentPath;
}

// ---- Test seams ----

void PropertiesView::setRowValueForTest(const QString &key, const QString &text)
{
    const int i = indexOfKey(key);
    if (i < 0 || m_rows[i]->isReadOnly()) return;
    // Find the value editor (PropertyEditorWidget) and set via its own QLineEdit,
    // not the key-rename QLineEdit which appears first in the child tree.
    if (auto *editor = m_rows[i]->findChild<PropertyEditorWidget *>()) {
        if (auto *le = editor->findChild<QLineEdit *>()) le->setText(text);
    }
}

// ---- Private helpers ----

int PropertiesView::indexOfKey(const QString &key) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows[i]->key() == key) return i;
    return -1;
}

bool PropertiesView::keyExists(const QString &key) const
{
    for (PropertyRow *r : m_rows)
        if (r->key().compare(key, Qt::CaseInsensitive) == 0) return true;
    return false;
}

} // namespace Corbomite
