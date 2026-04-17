// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesView.h"

#include "../../sidebar/PropertyEditorWidget.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/models/PropertyType.h"
#include "corbomite/models/PropertyTypeInference.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>

#include <QDebug>
#include <QFont>
#include <QFormLayout>
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
    , m_form(nullptr)
    , m_formContainer(nullptr)
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

    m_formContainer = new QWidget(this);
    m_form = new QFormLayout(m_formContainer);
    m_form->setContentsMargins(0, 0, 0, 0);
    m_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->addWidget(m_formContainer);

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

void PropertiesView::onEditorValueChanged() { scheduleWrite(); }

void PropertiesView::onAddPropertyClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("Add property"),
        i18n("Property name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    addPropertyNamed(name.trimmed());
}

void PropertiesView::addPropertyNamed(const QString &name)
{
    if (name.isEmpty() || m_currentPath.isEmpty()) return;
    for (const auto &row : m_rows) {
        if (row.key == name) return;
    }
    auto *editor = makePropertyEditor(PropertyType::Text,
        Markoff::YamlValue::emptyMap().get(QStringLiteral("_")), m_formContainer);
    connect(editor, &PropertyEditorWidget::valueChanged, this,
            &PropertiesView::onEditorValueChanged);
    m_form->addRow(name, editor);
    m_rows.push_back({name, editor});
    m_emptyLabel->setVisible(false);
    m_formContainer->setVisible(true);
    scheduleWrite();
}

void PropertiesView::clearEditors()
{
    for (auto &row : m_rows) {
        if (row.editor) m_form->removeRow(row.editor);
    }
    m_rows.clear();
}

void PropertiesView::refresh()
{
    clearEditors();

    if (m_currentPath.isEmpty() || !m_metadata) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_formContainer->setVisible(false);
        m_addPropertyButton->setEnabled(false);
        return;
    }
    m_addPropertyButton->setEnabled(true);
    const QJsonObject fm = m_metadata->frontmatterFor(m_currentPath);
    if (fm.isEmpty()) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_formContainer->setVisible(false);
        return;
    }
    m_emptyLabel->setVisible(false);
    m_formContainer->setVisible(true);
    m_headerLabel->setText(i18n("Properties (%1)", fm.size()));

    for (auto it = fm.begin(); it != fm.end(); ++it) {
        const QString key = it.key();
        const Markoff::YamlValue yval = qJsonValueToYaml(it.value());
        const PropertyType type = inferPropertyType(yval);
        auto *editor = makePropertyEditor(type, yval, m_formContainer);
        connect(editor, &PropertyEditorWidget::valueChanged, this,
                &PropertiesView::onEditorValueChanged);
        m_form->addRow(key, editor);
        m_rows.push_back({key, editor});
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
    QVector<EditorRow> snapshot = m_rows;
    const bool ok = m_fmProxy->processFrontMatter(tf, [&snapshot](QVariantMap &fm) {
        for (const auto &row : snapshot) {
            if (!row.editor) continue;
            const Markoff::YamlValue v = row.editor->currentValue();
            switch (v.kind()) {
            case Markoff::YamlValue::Kind::Bool:    fm[row.key] = v.asBool(); break;
            case Markoff::YamlValue::Kind::Int:     fm[row.key] = QVariant::fromValue<qlonglong>(v.asInt()); break;
            case Markoff::YamlValue::Kind::Double:  fm[row.key] = v.asDouble(); break;
            case Markoff::YamlValue::Kind::String:  fm[row.key] = v.asString(); break;
            case Markoff::YamlValue::Kind::Seq:     fm[row.key] = v.asStringList(); break;
            case Markoff::YamlValue::Kind::Null:
            default:                                fm[row.key] = QVariant(); break;
            }
        }
    });
    if (!ok) {
        qWarning() << "PropertiesView: processFrontMatter failed for" << m_currentPath;
    }
}

} // namespace Corbomite
