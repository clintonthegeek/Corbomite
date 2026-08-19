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

CanvasFilePickerDialog::CanvasFilePickerDialog(const QStringList &candidatePaths, QWidget *parent)
    : QDialog(parent)
    , m_candidates(candidatePaths)
{
    setWindowTitle(i18n("Insert file card"));

    auto *layout = new QVBoxLayout(this);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(i18n("Type to search vault files…"));
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

    if (filterText.isEmpty()) {
        for (const auto &path : std::as_const(m_candidates))
            m_resultsList->addItem(path);
        if (m_resultsList->count() > 0)
            m_resultsList->setCurrentRow(0);
        return;
    }

    const auto query = Corbomite::FuzzyMatcher::prepareQuery(filterText);

    struct Ranked { QString path; double score; };
    QVector<Ranked> ranked;
    ranked.reserve(m_candidates.size());
    for (const auto &path : std::as_const(m_candidates)) {
        if (auto match = Corbomite::FuzzyMatcher::fuzzySearch(query, path))
            ranked.append({path, match->score});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        return a.score > b.score;
    });

    for (const auto &r : std::as_const(ranked))
        m_resultsList->addItem(r.path);
    if (m_resultsList->count() > 0)
        m_resultsList->setCurrentRow(0);
}

void CanvasFilePickerDialog::acceptCurrentSelection()
{
    if (auto *item = m_resultsList->currentItem()) {
        m_selectedPath = item->text();
        accept();
    } else {
        reject();
    }
}

QString CanvasFilePickerDialog::selectedPath() const
{
    return m_selectedPath;
}

QString CanvasFilePickerDialog::pickFile(QWidget *parent, const QStringList &candidatePaths)
{
    CanvasFilePickerDialog dlg(candidatePaths, parent);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.selectedPath();
    return QString();
}

} // namespace Canvas
