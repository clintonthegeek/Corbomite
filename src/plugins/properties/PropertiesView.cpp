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

#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDropEvent>
#include <QEvent>
#include <QFont>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
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
    m_rowsContainer->setAcceptDrops(true);
    m_rowsContainer->installEventFilter(this);
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
    QDialog dlg(this);
    dlg.setWindowTitle(i18n("Add property"));
    auto *form = new QFormLayout(&dlg);
    auto *nameEdit = new QLineEdit(&dlg);
    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(i18n("Text"),        static_cast<int>(PropertyType::Text));
    typeCombo->addItem(i18n("Number"),      static_cast<int>(PropertyType::Number));
    typeCombo->addItem(i18n("Checkbox"),    static_cast<int>(PropertyType::Checkbox));
    typeCombo->addItem(i18n("Date"),        static_cast<int>(PropertyType::Date));
    typeCombo->addItem(i18n("Date & time"), static_cast<int>(PropertyType::DateTime));
    typeCombo->addItem(i18n("List"),        static_cast<int>(PropertyType::List));
    form->addRow(i18n("Name:"), nameEdit);
    form->addRow(i18n("Type:"), typeCombo);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) return;
    if (keyExists(name)) {
        QMessageBox::warning(this, i18n("Add property"),
            i18n("A property named '%1' already exists.", name));
        return;
    }
    addProperty(name, static_cast<PropertyType>(typeCombo->currentData().toInt()));
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

bool PropertiesView::renameProperty(const QString &oldKey, const QString &newKey)
{
    const QString n = newKey.trimmed();
    const int i = indexOfKey(oldKey);
    if (i < 0 || n.isEmpty()) return false;
    if (m_rows[i]->isReadOnly()) return false;
    if (keyExists(n) && n.compare(oldKey, Qt::CaseInsensitive) != 0) return false;

    PropertyRow *old = m_rows[i];
    const PropertyType type = old->type();
    const Markoff::YamlValue val = old->currentValue();
    auto *row = new PropertyRow(n, type, val, /*editable=*/true, m_rowsContainer);
    connectRow(row);
    m_rowsLayout->insertWidget(i, row);
    m_rowsLayout->removeWidget(old);
    old->deleteLater();
    m_rows[i] = row;
    reindexRows();
    scheduleWrite();
    return true;
}

void PropertiesView::deleteProperty(const QString &key)
{
    const int i = indexOfKey(key);
    if (i < 0) return;
    PropertyRow *row = m_rows.takeAt(i);
    m_rowsLayout->removeWidget(row);
    row->deleteLater();
    if (m_rows.isEmpty()) { m_emptyLabel->setVisible(true); m_rowsContainer->setVisible(false); }
    reindexRows();
    scheduleWrite();
}

void PropertiesView::moveProperty(int from, int to)
{
    if (from < 0 || from >= m_rows.size() || to < 0 || to >= m_rows.size() || from == to)
        return;
    PropertyRow *row = m_rows.takeAt(from);
    m_rows.insert(to, row);
    m_rowsLayout->removeWidget(row);
    m_rowsLayout->insertWidget(to, row);
    reindexRows();
    scheduleWrite();
}

// ---- Drop event handling ----

bool PropertiesView::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_rowsContainer) {
        if (ev->type() == QEvent::DragEnter) {
            auto *de = static_cast<QDragEnterEvent *>(ev);
            if (de->mimeData()->hasFormat(
                    QStringLiteral("application/x-corbomite-property-row"))) {
                de->acceptProposedAction();
                return true;
            }
        } else if (ev->type() == QEvent::Drop) {
            auto *de = static_cast<QDropEvent *>(ev);
            if (!de->mimeData()->hasFormat(
                    QStringLiteral("application/x-corbomite-property-row")))
                return false;
            const int srcIndex = de->mimeData()
                ->data(QStringLiteral("application/x-corbomite-property-row"))
                .toInt();
            // Determine target index from the child widget under the drop point.
            const QPoint localPos = de->position().toPoint();
            int targetIndex = m_rows.size() - 1;  // default: drop at end
            if (auto *child = m_rowsContainer->childAt(localPos)) {
                // Walk up until we find a direct PropertyRow child.
                QWidget *w = child;
                while (w && w->parent() != m_rowsContainer)
                    w = qobject_cast<QWidget *>(w->parent());
                if (auto *row = qobject_cast<PropertyRow *>(w))
                    targetIndex = row->visualIndex();
            }
            de->acceptProposedAction();
            moveProperty(srcIndex, targetIndex);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// ---- Row management ----

void PropertiesView::clearRows()
{
    for (PropertyRow *r : m_rows) { m_rowsLayout->removeWidget(r); r->deleteLater(); }
    m_rows.clear();
}

void PropertiesView::connectRow(PropertyRow *row)
{
    connect(row, &PropertyRow::valueChanged, this, &PropertiesView::scheduleWrite);
    connect(row, &PropertyRow::deleteRequested, this,
            [this, row]() { deleteProperty(row->key()); });
    connect(row, &PropertyRow::keyRenameRequested, this,
            [this](const QString &o, const QString &n) { renameProperty(o, n); });
}

void PropertiesView::reindexRows()
{
    for (int i = 0; i < m_rows.size(); ++i)
        m_rows[i]->setVisualIndex(i);
}

void PropertiesView::appendRow(const QString &key, PropertyType type,
                               const Markoff::YamlValue &value, bool editable)
{
    auto *row = new PropertyRow(key, type, value, editable, m_rowsContainer);
    connectRow(row);
    m_rowsLayout->addWidget(row);
    m_rows.push_back(row);
    reindexRows();
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
