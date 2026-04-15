// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertiesPanel.h"

#include "PropertyEditorWidget.h"

#include "corbomite/core/FrontMatterWriter.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/PropertyType.h"
#include "corbomite/models/PropertyTypeInference.h"
#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/MetadataCache.h"

#include <KLocalizedString>

#include <QDebug>
#include <QFont>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace Corbomite {

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
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
    connect(m_writeDebounce, &QTimer::timeout,
            this, &PropertiesPanel::flushWrite);

    connect(m_addPropertyButton, &QPushButton::clicked,
            this, &PropertiesPanel::onAddPropertyClicked);

    refresh();
}

PropertiesPanel::~PropertiesPanel() = default;

void PropertiesPanel::setMetadataCache(MetadataCache *cache)
{
    if (m_cache) {
        disconnect(m_cache, nullptr, this, nullptr);
    }
    m_cache = cache;
    if (m_cache) {
        connect(m_cache, &MetadataCache::cacheChanged,
                this, [this](const QString &path, const QString &,
                             const Corbomite::CachedMetadata &) {
                    onMetadataCacheChanged(path);
                });
    }
    refresh();
}

void PropertiesPanel::setCurrentNote(NoteDocument *doc)
{
    m_currentDoc = doc;
    // Cancel any pending write from a previous document — safer than
    // flushing (previous doc may have been closed).
    m_writeDebounce->stop();
    refresh();
}

int PropertiesPanel::rowCount() const
{
    return m_rows.size();
}

void PropertiesPanel::flushPendingWrite()
{
    if (m_writeDebounce->isActive()) {
        m_writeDebounce->stop();
        flushWrite();
    }
}

void PropertiesPanel::onMetadataCacheChanged(const QString &path)
{
    if (!m_currentDoc) return;
    if (path != m_currentDoc->relativePath()) return;
    // Suppress refresh while a user edit is pending — otherwise our own
    // write-back will reparse and yank the user's in-progress edits.
    if (m_writeDebounce->isActive()) return;
    refresh();
}

void PropertiesPanel::onEditorValueChanged()
{
    scheduleWrite();
}

void PropertiesPanel::onAddPropertyClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               i18n("Add property"),
                                               i18n("Property name:"),
                                               QLineEdit::Normal,
                                               QString(),
                                               &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    addPropertyNamed(name.trimmed());
}

void PropertiesPanel::addPropertyNamed(const QString &name)
{
    if (name.isEmpty() || !m_currentDoc) return;

    // Reject duplicates.
    for (const auto &row : m_rows) {
        if (row.key == name) return;
    }

    // Create an empty Text editor and insert it.
    auto *editor = makePropertyEditor(PropertyType::Text,
                                      Markoff::YamlValue::emptyMap().get(QStringLiteral("_")),
                                      m_formContainer);
    connect(editor, &PropertyEditorWidget::valueChanged,
            this, &PropertiesPanel::onEditorValueChanged);
    m_form->addRow(name, editor);
    m_rows.push_back({name, editor});

    m_emptyLabel->setVisible(false);
    m_formContainer->setVisible(true);

    scheduleWrite();
}

void PropertiesPanel::clearEditors()
{
    // Remove all rows from the form layout and delete the editor widgets.
    for (auto &row : m_rows) {
        if (row.editor) {
            m_form->removeRow(row.editor);  // deletes the row's widgets
        }
    }
    m_rows.clear();
}

void PropertiesPanel::refresh()
{
    clearEditors();

    if (!m_currentDoc) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_formContainer->setVisible(false);
        m_addPropertyButton->setEnabled(false);
        return;
    }

    m_addPropertyButton->setEnabled(true);

    std::optional<CachedMetadata> fileCache;
    if (m_cache) {
        fileCache = m_cache->getFileCache(m_currentDoc->relativePath());
    }

    if (!fileCache || !fileCache->frontmatter || fileCache->frontmatter->isEmpty()) {
        m_headerLabel->setText(i18n("Properties"));
        m_emptyLabel->setVisible(true);
        m_formContainer->setVisible(false);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_formContainer->setVisible(true);

    const QJsonObject &fm = *fileCache->frontmatter;
    m_headerLabel->setText(i18n("Properties (%1)", fm.size()));

    // Stable key ordering: QJsonObject iteration is key-sorted by QJSON.
    for (auto it = fm.begin(); it != fm.end(); ++it) {
        const QString key = it.key();
        const Markoff::YamlValue yval = qJsonValueToYaml(it.value());
        const PropertyType type = inferPropertyType(yval);
        auto *editor = makePropertyEditor(type, yval, m_formContainer);
        connect(editor, &PropertyEditorWidget::valueChanged,
                this, &PropertiesPanel::onEditorValueChanged);
        m_form->addRow(key, editor);
        m_rows.push_back({key, editor});
    }
}

void PropertiesPanel::scheduleWrite()
{
    m_writeDebounce->start();
}

void PropertiesPanel::flushWrite()
{
    if (!m_currentDoc) return;

    const QString filePath = m_currentDoc->filePath();
    if (filePath.isEmpty()) return;

    QVector<EditorRow> snapshot = m_rows;

    QString err;
    const bool ok = FrontMatterWriter::process(
        filePath,
        [&snapshot](Markoff::YamlValue &fm) {
            // Remove keys not currently present in the editor snapshot
            // (handles deletion via UI in a future iteration). For now,
            // we preserve unknown keys by not touching them. Just
            // overwrite known keys.
            for (const auto &row : snapshot) {
                if (!row.editor) continue;
                const Markoff::YamlValue v = row.editor->currentValue();
                switch (v.kind()) {
                case Markoff::YamlValue::Kind::Bool:
                    fm.setBool(row.key, v.asBool());
                    break;
                case Markoff::YamlValue::Kind::Int:
                    fm.setInt(row.key, v.asInt());
                    break;
                case Markoff::YamlValue::Kind::Double:
                    fm.setDouble(row.key, v.asDouble());
                    break;
                case Markoff::YamlValue::Kind::String:
                    fm.setString(row.key, v.asString());
                    break;
                case Markoff::YamlValue::Kind::Seq:
                    fm.setSeq(row.key, v.asStringList());
                    break;
                case Markoff::YamlValue::Kind::Null:
                default:
                    fm.setNull(row.key);
                    break;
                }
            }
        },
        &err);

    if (!ok) {
        qWarning() << "PropertiesPanel: FrontMatterWriter::process failed:"
                   << err << "for" << filePath;
        return;
    }

    Q_EMIT propertiesWritten(filePath);
}

}  // namespace Corbomite
