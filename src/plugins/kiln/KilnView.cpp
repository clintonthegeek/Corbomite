// SPDX-License-Identifier: GPL-3.0-or-later
#include "KilnView.h"

#include "KilnRunner.h"

#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace Corbomite {
namespace {

/// Lists are edited as one comma-separated line. A list editor for four different
/// lists would be more widget than feature, and these are folder names and note
/// names, which nobody types in bulk.
QString joinList(const QStringList &values)
{
    return values.join(QStringLiteral(", "));
}

QStringList splitList(const QString &text)
{
    QStringList out;
    for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            out.append(trimmed);
    }
    return out;
}

/// `series=series, author=authors` — one property per taxonomy, and a bare name means the
/// taxonomy is called what the property is.
QString joinTaxonomies(const QMap<QString, QString> &taxonomies)
{
    QStringList parts;
    for (auto it = taxonomies.constBegin(); it != taxonomies.constEnd(); ++it)
        parts.append(it.key() == it.value() ? it.key() : it.key() + QLatin1Char('=') + it.value());
    return parts.join(QStringLiteral(", "));
}

QMap<QString, QString> splitTaxonomies(const QString &text)
{
    QMap<QString, QString> out;
    for (const QString &part : splitList(text)) {
        const int equals = part.indexOf(QLatin1Char('='));
        if (equals > 0)
            out.insert(part.left(equals).trimmed(), part.mid(equals + 1).trimmed());
        else
            out.insert(part, part);
    }
    return out;
}

} // namespace

KilnView::KilnView(PluginContext *context, KilnRunner *runner, QWidget *parent)
    : QWidget(parent)
    , m_context(context)
    , m_runner(runner)
{
    m_vaultPath = context && context->vault() ? context->vault()->basePath() : QString();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &KilnView::saveSettings);

    // ---- engine and target
    m_enginePath = new QLineEdit(this);
    m_enginePath->setPlaceholderText(i18n("kiln"));
    auto *browseEngine = new QPushButton(i18n("Browse…"), this);
    auto *engineRow = new QHBoxLayout;
    engineRow->addWidget(m_enginePath, 1);
    engineRow->addWidget(browseEngine);

    m_output = new QLineEdit(this);
    m_output->setPlaceholderText(i18n("the Hugo site to write content/ into"));
    auto *browseOutput = new QPushButton(i18n("Browse…"), this);
    auto *revealOutput = new QPushButton(i18n("Reveal"), this);
    auto *outputRow = new QHBoxLayout;
    outputRow->addWidget(m_output, 1);
    outputRow->addWidget(browseOutput);
    outputRow->addWidget(revealOutput);

    auto *engineForm = new QFormLayout;
    engineForm->addRow(i18n("Engine"), engineRow);
    engineForm->addRow(i18n("Site directory"), outputRow);
    auto *engineBox = new QGroupBox(i18n("Engine"), this);
    engineBox->setLayout(engineForm);

    // ---- projections
    m_projectionList = new QListWidget(this);
    auto *addButton = new QPushButton(i18n("Add"), this);
    auto *removeButton = new QPushButton(i18n("Remove"), this);
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(addButton);
    listButtons->addWidget(removeButton);
    listButtons->addStretch(1);

    auto *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(new QLabel(i18n("Projections"), this));
    leftColumn->addWidget(m_projectionList, 1);
    leftColumn->addLayout(listButtons);

    m_name = new QLineEdit(this);
    m_folders = new QLineEdit(this);
    m_folders->setPlaceholderText(i18n("software, techniques"));
    m_deny = new QLineEdit(this);
    m_deny->setPlaceholderText(i18n("_schema.md, Private.md"));
    m_orderFrom = new QLineEdit(this);
    m_orderFrom->setPlaceholderText(i18n("a note whose links, in order, are the running order"));
    m_section = new QLineEdit(this);
    m_section->setPlaceholderText(i18n("empty uses each note's own folder"));
    m_section->setToolTip(i18n("The section to publish into. Empty means a note's own folder is its section, "
                               "so `software/` and `techniques/` publish into both. Set it to move a folder's "
                               "notes elsewhere — the folders inside that one then nest under it."));
    m_shape = new QComboBox(this);
    m_shape->addItems({QStringLiteral("bundle"), QStringLiteral("leaf")});
    m_frontMatter = new QComboBox(this);
    m_frontMatter->addItems({QStringLiteral("yaml"), QStringLiteral("toml")});
    m_fieldOrder = new QLineEdit(this);
    m_fieldOrder->setPlaceholderText(i18n("empty keeps each note's own order"));
    m_linkBase = new QLineEdit(this);
    m_linkBase->setPlaceholderText(i18n("empty derives it from the section"));
    m_taxonomies = new QLineEdit(this);
    m_taxonomies->setPlaceholderText(i18n("series, author=authors"));
    m_related = new QCheckBox(i18n("Let Hugo work out related pages"), this);
    m_related->setToolTip(i18n("From shared tags, keywords and series — no list to maintain"));
    m_fragments = new QCheckBox(i18n("Write Kiln's config fragments"), this);
    m_fragments->setToolTip(i18n("config/_default/markup.toml and friends; Kiln rewrites only its own"));
    m_scaffold = new QCheckBox(i18n("Write the site shell too"), this);
    m_scaffold->setToolTip(i18n("Layouts, render hooks and a stylesheet, written once"));

    auto *projectionForm = new QFormLayout;
    projectionForm->addRow(i18n("Name"), m_name);
    projectionForm->addRow(i18n("Folders"), m_folders);
    projectionForm->addRow(i18n("Held back"), m_deny);
    projectionForm->addRow(i18n("Running order"), m_orderFrom);
    projectionForm->addRow(i18n("Section"), m_section);
    projectionForm->addRow(i18n("Shape"), m_shape);
    projectionForm->addRow(i18n("Front matter"), m_frontMatter);
    projectionForm->addRow(i18n("Fields"), m_fieldOrder);
    projectionForm->addRow(i18n("Link base"), m_linkBase);
    projectionForm->addRow(i18n("Taxonomies"), m_taxonomies);
    projectionForm->addRow(QString(), m_related);
    projectionForm->addRow(QString(), m_fragments);
    projectionForm->addRow(QString(), m_scaffold);

    auto *projectionBox = new QGroupBox(i18n("Projection"), this);
    projectionBox->setLayout(projectionForm);

    auto *columns = new QHBoxLayout;
    columns->addLayout(leftColumn, 1);
    columns->addWidget(projectionBox, 2);

    // ---- actions and report
    m_checkButton = new QPushButton(i18n("Check"), this);
    m_checkButton->setToolTip(i18n("Report what would change, and write nothing"));
    m_projectButton = new QPushButton(i18n("Project"), this);
    m_projectButton->setToolTip(i18n("Write the pages"));
    m_scaffoldButton = new QPushButton(i18n("Write the site shell"), this);
    m_scaffoldButton->setToolTip(i18n("Layouts, render hooks and a stylesheet — written once, never overwritten"));
    auto *copyButton = new QPushButton(i18n("Copy config"), this);
    copyButton->setToolTip(i18n("The same JSON, to pipe to kiln yourself"));

    auto *actions = new QHBoxLayout;
    actions->addWidget(m_checkButton);
    actions->addWidget(m_projectButton);
    actions->addWidget(m_scaffoldButton);
    actions->addWidget(copyButton);
    actions->addStretch(1);

    m_report = new QPlainTextEdit(this);
    m_report->setReadOnly(true);
    QFont monospace(QStringLiteral("monospace"));
    monospace.setStyleHint(QFont::TypeWriter);
    m_report->setFont(monospace);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(engineBox);
    layout->addLayout(columns, 1);
    layout->addLayout(actions);
    layout->addWidget(m_report, 2);

    connect(m_projectionList, &QListWidget::currentRowChanged, this,
            &KilnView::onProjectionSelected);
    connect(addButton, &QPushButton::clicked, this, &KilnView::addProjection);
    connect(removeButton, &QPushButton::clicked, this, &KilnView::removeProjection);
    connect(m_checkButton, &QPushButton::clicked, this, &KilnView::runCheck);
    connect(m_projectButton, &QPushButton::clicked, this, &KilnView::runProject);
    connect(m_scaffoldButton, &QPushButton::clicked, this, &KilnView::runScaffold);
    connect(copyButton, &QPushButton::clicked, this, &KilnView::onCopyConfig);
    connect(browseEngine, &QPushButton::clicked, this, &KilnView::onBrowseEngine);
    connect(browseOutput, &QPushButton::clicked, this, &KilnView::onBrowseOutput);
    connect(revealOutput, &QPushButton::clicked, this, &KilnView::onRevealOutput);

    for (QLineEdit *field : {m_enginePath, m_output, m_name, m_folders, m_deny, m_orderFrom,
                             m_section, m_fieldOrder, m_linkBase, m_taxonomies}) {
        connect(field, &QLineEdit::textChanged, this, &KilnView::onFieldChanged);
    }
    for (QCheckBox *box : {m_related, m_fragments, m_scaffold}) {
        connect(box, &QCheckBox::toggled, this, &KilnView::onFieldChanged);
    }
    for (QComboBox *combo : {m_shape, m_frontMatter}) {
        connect(combo, &QComboBox::currentTextChanged, this, &KilnView::onFieldChanged);
    }
    if (m_runner) {
        connect(m_runner, &KilnRunner::finished, this, &KilnView::onRunFinished);
    }

    loadSettings();
    setReport(emptyStateText());
}

void KilnView::reloadFromStoreIfChanged()
{
    if (!m_context || m_loading)
        return;
    // An edit is still on its way to disk; reloading now would take the text out from
    // under the caret.
    if (m_saveTimer.isActive())
        return;

    // The host fires this on every change to data.json, including the panel's own
    // autosaves, so comparing first is what keeps a save from turning into a reload.
    const QJsonObject stored = m_context->loadData();
    if (stored == m_settings.toJson(m_vaultPath))
        return;

    const int previous = m_current;
    m_settings = KilnSettings::fromJson(stored);

    m_loading = true;
    m_enginePath->setText(m_settings.enginePath);
    m_output->setText(m_settings.output);
    m_loading = false;

    m_current = previous;
    rebuildProjectionList();
}

QString KilnView::emptyStateText() const
{
    if (m_settings.projections.isEmpty()) {
        return i18n("No projections yet.\n\n"
                    "A projection says which notes belong to a site. Add one and give it a "
                    "folder — the cohort is a top-level folder such as 'software'. Hold back "
                    "the notes that are working material rather than pages. For a book, give "
                    "it a running order instead: a map-of-content note whose links, in order, "
                    "are the chapters.\n\n"
                    "Check reports what would happen and writes nothing.");
    }
    if (!m_settings.problem().isEmpty())
        return m_settings.problem();
    return i18n("Ready. Check reports what would change; Project writes it.");
}

// ---------------------------------------------------------------- settings

void KilnView::loadSettings()
{
    m_settings = KilnSettings::fromJson(m_context ? m_context->loadData() : QJsonObject());

    m_loading = true;
    m_enginePath->setText(m_settings.enginePath);
    m_output->setText(m_settings.output);
    m_loading = false;

    rebuildProjectionList();
}

void KilnView::saveSettings()
{
    if (!m_context)
        return;
    m_context->saveData(m_settings.toJson(m_vaultPath));
}

void KilnView::rebuildProjectionList()
{
    const int previous = m_current;
    m_loading = true;
    m_projectionList->clear();
    for (const KilnProjection &projection : m_settings.projections)
        m_projectionList->addItem(projection.name);
    m_loading = false;

    if (m_settings.projections.isEmpty()) {
        m_current = -1;
        loadProjectionIntoForm(-1);
        return;
    }
    const int row = qBound(0, previous, m_settings.projections.size() - 1);
    m_projectionList->setCurrentRow(row);
}

void KilnView::loadProjectionIntoForm(int index)
{
    const bool valid = index >= 0 && index < m_settings.projections.size();
    for (QWidget *widget : {static_cast<QWidget *>(m_name), static_cast<QWidget *>(m_folders),
                            static_cast<QWidget *>(m_deny), static_cast<QWidget *>(m_orderFrom),
                            static_cast<QWidget *>(m_section), static_cast<QWidget *>(m_shape),
                            static_cast<QWidget *>(m_frontMatter),
                            static_cast<QWidget *>(m_fieldOrder),
                            static_cast<QWidget *>(m_linkBase),
                            static_cast<QWidget *>(m_taxonomies),
                            static_cast<QWidget *>(m_related),
                            static_cast<QWidget *>(m_fragments),
                            static_cast<QWidget *>(m_scaffold)}) {
        widget->setEnabled(valid);
    }
    if (!valid) {
        m_loading = true;
        m_name->clear();
        m_folders->clear();
        m_deny->clear();
        m_orderFrom->clear();
        m_section->clear();
        m_fieldOrder->clear();
        m_linkBase->clear();
        m_taxonomies->clear();
        m_related->setChecked(true);
        m_fragments->setChecked(true);
        m_scaffold->setChecked(false);
        m_loading = false;
        return;
    }

    const KilnProjection &projection = m_settings.projections.at(index);
    m_loading = true;
    m_name->setText(projection.name);
    m_folders->setText(joinList(projection.folders));
    m_deny->setText(joinList(projection.deny));
    m_orderFrom->setText(projection.orderFrom);
    m_section->setText(projection.section);
    m_shape->setCurrentText(projection.shape);
    m_frontMatter->setCurrentText(projection.frontMatter);
    m_fieldOrder->setText(joinList(projection.fieldOrder));
    m_linkBase->setText(projection.linkBase);
    m_taxonomies->setText(joinTaxonomies(projection.taxonomies));
    m_related->setChecked(projection.related);
    m_fragments->setChecked(projection.fragments);
    m_scaffold->setChecked(projection.scaffold);
    m_loading = false;
}

void KilnView::storeFormIntoProjection()
{
    if (m_current < 0 || m_current >= m_settings.projections.size())
        return;

    KilnProjection &projection = m_settings.projections[m_current];
    projection.name = m_name->text().trimmed();
    projection.folders = splitList(m_folders->text());
    projection.deny = splitList(m_deny->text());
    projection.orderFrom = m_orderFrom->text().trimmed();
    projection.section = m_section->text().trimmed();
    projection.shape = m_shape->currentText();
    projection.frontMatter = m_frontMatter->currentText();
    projection.fieldOrder = splitList(m_fieldOrder->text());
    projection.linkBase = m_linkBase->text().trimmed();
    projection.taxonomies = splitTaxonomies(m_taxonomies->text());
    projection.related = m_related->isChecked();
    projection.fragments = m_fragments->isChecked();
    projection.scaffold = m_scaffold->isChecked();

    // The row label follows the name without rebuilding the list, which would drop
    // the selection out from under the caret.
    if (QListWidgetItem *item = m_projectionList->item(m_current))
        item->setText(projection.name.isEmpty() ? i18n("(unnamed)") : projection.name);
}

void KilnView::onFieldChanged()
{
    if (m_loading)
        return;

    if (sender() == m_enginePath)
        m_settings.enginePath = m_enginePath->text().trimmed();
    else if (sender() == m_output)
        m_settings.output = m_output->text().trimmed();
    else
        storeFormIntoProjection();

    m_saveTimer.start();
}

void KilnView::onProjectionSelected(int row)
{
    if (m_loading)
        return;
    if (m_current >= 0 && m_current != row)
        storeFormIntoProjection();
    m_current = row;
    loadProjectionIntoForm(row);
}

void KilnView::addProjection()
{
    storeFormIntoProjection();

    KilnProjection projection;
    projection.name = i18n("site %1", m_settings.projections.size() + 1);
    m_settings.projections.append(projection);
    m_current = m_settings.projections.size() - 1;
    rebuildProjectionList();
    saveSettings();

    m_name->setFocus();
    m_name->selectAll();
}

void KilnView::removeProjection()
{
    if (m_current < 0 || m_current >= m_settings.projections.size())
        return;

    m_settings.projections.removeAt(m_current);
    m_current = qMin(m_current, m_settings.projections.size() - 1);
    rebuildProjectionList();
    saveSettings();
    setReport(emptyStateText());
}

// ---------------------------------------------------------------- actions

void KilnView::onBrowseEngine()
{
    const QString path = QFileDialog::getOpenFileName(this, i18n("Kiln engine"), m_enginePath->text());
    if (!path.isEmpty())
        m_enginePath->setText(path);
}

void KilnView::onBrowseOutput()
{
    const QString path =
        QFileDialog::getExistingDirectory(this, i18n("Site directory"), m_output->text());
    if (!path.isEmpty())
        m_output->setText(path);
}

void KilnView::onRevealOutput()
{
    if (!m_settings.output.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_settings.output));
}

void KilnView::onCopyConfig()
{
    storeFormIntoProjection();
    saveSettings();
    const QJsonObject config = m_settings.toJson(m_vaultPath);
    QGuiApplication::clipboard()->setText(
        QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Indented)));
    appendReport(i18n("Config copied. The engine reads the same JSON: "
                      "kiln check --config - < config.json"));
}

void KilnView::runCheck()
{
    run(QStringLiteral("check"));
}

void KilnView::runProject()
{
    run(QStringLiteral("project"));
}

void KilnView::runScaffold()
{
    // Writes the site shell and no content, so a site builds before there is anything to
    // put in it. Nothing that already exists is overwritten.
    run(QStringLiteral("scaffold"));
}

void KilnView::run(const QString &mode)
{
    storeFormIntoProjection();
    saveSettings();

    // Checked before anything is spawned, so a half-filled projection is a sentence
    // here rather than a process failing somewhere else.
    const QString problem = m_settings.problem();
    if (!problem.isEmpty()) {
        setReport(problem);
        return;
    }
    if (!m_runner) {
        setReport(i18n("No runner: the plugin has no process permission."));
        return;
    }

    setBusy(true);
    setReport(mode == QLatin1String("check") ? i18n("Checking…") : i18n("Projecting…"));

    const QJsonObject config = m_settings.toJson(m_vaultPath);
    if (!m_runner->run(m_settings.enginePath, mode, config)) {
        setBusy(false);
        setReport(i18n("Could not start '%1'. Check the engine path.", m_settings.enginePath));
    }
}

void KilnView::onRunFinished(int exitCode, const QJsonObject &report, const QString &standardError)
{
    setBusy(false);
    setReport(describeReport(report, exitCode, standardError));
}

void KilnView::setBusy(bool busy)
{
    m_checkButton->setEnabled(!busy);
    m_projectButton->setEnabled(!busy);
    m_scaffoldButton->setEnabled(!busy);
}

void KilnView::setReport(const QString &text)
{
    m_report->setPlainText(text);
}

void KilnView::appendReport(const QString &text)
{
    m_report->appendPlainText(text);
}

QJsonObject KilnView::configForRun() const
{
    return m_settings.toJson(m_vaultPath);
}

QString KilnView::reportText() const
{
    return m_report->toPlainText();
}

} // namespace Corbomite
