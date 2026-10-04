// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasFilePickerDialog.h"

#include <corbomite/search/FuzzyMatch.h>
#include <corbomite/search/FuzzyMatcher.h>
#include <corbomite/search/PreparedQuery.h>

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace Canvas {

CanvasFilePickerDialog::CanvasFilePickerDialog(const QVector<Entry> &entries, QWidget *parent,
                                               const QString &title, const QString &placeholder)
    : QDialog(parent)
    , m_entries(entries)
{
    setWindowTitle(title);

    auto *layout = new QVBoxLayout(this);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(placeholder);
    layout->addWidget(m_filterEdit);

    m_resultsList = new QListWidget(this);
    layout->addWidget(m_resultsList);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &CanvasFilePickerDialog::updateResults);
    connect(m_resultsList, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        acceptCurrentSelection();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &CanvasFilePickerDialog::acceptCurrentSelection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateResults(QString());
    m_filterEdit->setFocus();
}

void CanvasFilePickerDialog::updateResults(const QString &filterText)
{
    m_resultsList->clear();

    const auto addRow = [this](const Entry &entry) {
        auto *item = new QListWidgetItem(entry.display, m_resultsList);
        item->setData(Qt::UserRole, entry.payload);
    };

    if (filterText.isEmpty()) {
        for (const auto &entry : std::as_const(m_entries))
            addRow(entry);
        if (m_resultsList->count() > 0)
            m_resultsList->setCurrentRow(0);
        return;
    }

    const auto query = Corbomite::FuzzyMatcher::prepareQuery(filterText);

    struct Ranked { Entry entry; double score; };
    QVector<Ranked> ranked;
    ranked.reserve(m_entries.size());
    for (const auto &entry : std::as_const(m_entries)) {
        if (auto match = Corbomite::FuzzyMatcher::fuzzySearch(query, entry.display))
            ranked.append({entry, match->score});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        return a.score > b.score;
    });

    for (const auto &r : std::as_const(ranked))
        addRow(r.entry);
    if (m_resultsList->count() > 0)
        m_resultsList->setCurrentRow(0);
}

void CanvasFilePickerDialog::acceptCurrentSelection()
{
    if (auto *item = m_resultsList->currentItem()) {
        m_selectedPayload = item->data(Qt::UserRole).toString();
        accept();
    } else {
        reject();
    }
}

QString CanvasFilePickerDialog::selectedPath() const
{
    return m_selectedPayload;
}

QString CanvasFilePickerDialog::selectedPayload() const
{
    return m_selectedPayload;
}

QString CanvasFilePickerDialog::pickFile(QWidget *parent, const QStringList &candidatePaths)
{
    QVector<Entry> entries;
    entries.reserve(candidatePaths.size());
    for (const auto &path : candidatePaths)
        entries.append({path, path});
    return pickPayload(parent, entries, i18n("Insert file card"),
                       i18n("Type to search vault files…"));
}

QString CanvasFilePickerDialog::pickPayload(QWidget *parent, const QVector<Entry> &entries,
                                            const QString &title, const QString &placeholder)
{
    CanvasFilePickerDialog dlg(entries, parent, title, placeholder);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.selectedPayload();
    return QString();
}

} // namespace Canvas