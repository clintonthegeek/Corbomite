// SPDX-License-Identifier: GPL-3.0-or-later
#include "MergeFileDialog.h"

#include "corbomite/search/FuzzyMatcher.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace Corbomite {

MergeFileDialog::MergeFileDialog(QString sourceRelativePath,
                                 QStringList candidatePaths,
                                 QWidget *parent)
    : QDialog(parent)
    , m_source(std::move(sourceRelativePath))
{
    setWindowTitle(i18n("Merge '%1' with…", m_source));
    resize(560, 420);

    m_candidates = candidatePaths;
    m_candidates.removeAll(m_source);  // never offer the source itself

    // ---- Stage 1: fuzzy picker ----
    auto *pick = new QWidget(this);
    auto *pickLayout = new QVBoxLayout(pick);
    pickLayout->setContentsMargins(0, 0, 0, 0);

    m_filter = new QLineEdit(pick);
    m_filter->setObjectName(QStringLiteral("mergeFilter"));
    m_filter->setPlaceholderText(i18n("Type to filter files…"));
    pickLayout->addWidget(m_filter);

    m_list = new QListWidget(pick);
    m_list->setObjectName(QStringLiteral("mergeList"));
    m_list->addItems(m_candidates);
    pickLayout->addWidget(m_list);

    // ---- Stage 2: append/prepend confirm ----
    auto *confirm = new QWidget(this);
    auto *confirmLayout = new QVBoxLayout(confirm);
    confirmLayout->setContentsMargins(0, 0, 0, 0);

    m_confirmText = new QLabel(confirm);
    m_confirmText->setObjectName(QStringLiteral("mergeConfirmText"));
    m_confirmText->setWordWrap(true);
    confirmLayout->addWidget(m_confirmText);

    // Q3: warn when the source frontmatter will be discarded; P3.5: the
    // link-rewrite rough edge is copy, not code.
    auto *caveat = new QLabel(i18n(
        "The source note's frontmatter is not copied, and links to it are "
        "not updated."), confirm);
    caveat->setWordWrap(true);
    confirmLayout->addWidget(caveat);

    m_append = new QRadioButton(i18n("Append to the end"), confirm);
    m_append->setObjectName(QStringLiteral("mergeAppend"));
    m_append->setChecked(true);
    m_prepend = new QRadioButton(i18n("Prepend to the start"), confirm);
    m_prepend->setObjectName(QStringLiteral("mergePrepend"));
    confirmLayout->addWidget(m_append);
    confirmLayout->addWidget(m_prepend);
    confirmLayout->addStretch();

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("mergeStack"));
    m_stack->addWidget(pick);
    m_stack->addWidget(confirm);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *next = buttons->addButton(i18n("Next"), QDialogButtonBox::ActionRole);
    next->setObjectName(QStringLiteral("mergeNext"));
    auto *merge = buttons->addButton(i18n("Merge"), QDialogButtonBox::AcceptRole);
    merge->setObjectName(QStringLiteral("mergeConfirm"));
    merge->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_stack);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_filter, &QLineEdit::textChanged,
            this, &MergeFileDialog::applyFilter);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) {
                if (item) showConfirmFor(item->text());
            });
    connect(next, &QPushButton::clicked, this, [this] {
        if (auto *item = m_list->currentItem())
            showConfirmFor(item->text());
    });
    connect(merge, &QPushButton::clicked, this, [this] {
        if (!m_selected.isEmpty())
            accept();
    });
}

bool MergeFileDialog::prependSelected() const
{
    return m_prepend && m_prepend->isChecked();
}

void MergeFileDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    m_list->clear();
    if (needle.isEmpty()) {
        m_list->addItems(m_candidates);
        return;
    }
    const PreparedQuery query = FuzzyMatcher::prepareQuery(needle);
    for (const QString &path : m_candidates) {
        if (FuzzyMatcher::fuzzySearch(query, path).has_value())
            m_list->addItem(path);
    }
}

void MergeFileDialog::showConfirmFor(const QString &target)
{
    m_selected = target;
    m_confirmText->setText(i18n(
        "This will copy all content from '%1' into '%2' and delete '%1'.",
        m_source, target));
    // The Merge button lives in the button box; find it by object name.
    if (auto *merge = findChild<QPushButton *>(QStringLiteral("mergeConfirm")))
        merge->setEnabled(true);
    m_stack->setCurrentIndex(1);
}

}  // namespace Corbomite
