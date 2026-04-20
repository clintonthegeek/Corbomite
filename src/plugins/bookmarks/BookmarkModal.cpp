// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarkModal.h"

#include "BookmarksStore.h"

#include <KLocalizedString>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

#include <functional>

namespace Corbomite::Bookmarks {

BookmarkModal::BookmarkModal(BookmarkItem inferred, BookmarksStore *store,
                             QWidget *parent)
    : QDialog(parent), m_inferred(std::move(inferred)), m_store(store)
{
    setWindowTitle(i18n("New bookmark"));

    auto *outer = new QVBoxLayout(this);
    auto *form  = new QFormLayout;

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(inferDefaultTitle(m_inferred));
    form->addRow(i18n("Name:"), m_nameEdit);

    m_groupCombo = new QComboBox(this);
    const auto groups = collectGroups(m_store);
    for (const auto &g : groups) {
        m_groupCombo->addItem(g.first);
        m_groupPaths.append(g.second);
    }
    form->addRow(i18n("Group:"), m_groupCombo);

    outer->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(i18n("Save"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        commit();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

BookmarkModal::~BookmarkModal() = default;

QString BookmarkModal::inferDefaultTitle(const BookmarkItem &item)
{
    if (!item.title.isEmpty()) return item.title;
    if (item.type == QLatin1String("search"))
        return item.query.isEmpty() ? i18n("Search") : item.query;
    if (item.type == QLatin1String("graph"))
        return i18n("Graph view");
    if (!item.path.isEmpty()) {
        // Strip any #subpath before taking the basename.
        QString base = item.path;
        const int hash = base.indexOf(QLatin1Char('#'));
        if (hash >= 0) base = base.left(hash);
        QFileInfo fi(base);
        const QString stem = fi.completeBaseName();
        return stem.isEmpty() ? base : stem;
    }
    return QString();
}

QList<QPair<QString, QStringList>>
BookmarkModal::collectGroups(const BookmarksStore *store)
{
    QList<QPair<QString, QStringList>> out;
    out.append({i18n("(root)"), {}});
    if (!store) return out;

    std::function<void(const QList<BookmarkItem> &, const QStringList &, const QString &)>
        walk = [&](const QList<BookmarkItem> &items, const QStringList &basePath,
                   const QString &prefix) {
        for (int i = 0; i < items.size(); ++i) {
            if (items.at(i).type != QLatin1String("group")) continue;
            QStringList path = basePath;
            path.append(QString::number(i));
            const QString label = prefix.isEmpty()
                                      ? items.at(i).title
                                      : prefix + QLatin1String(" / ") + items.at(i).title;
            out.append({label, path});
            walk(items.at(i).children, path, label);
        }
    };
    walk(store->rootItems(), {}, QString());
    return out;
}

BookmarkItem BookmarkModal::composedItem() const
{
    BookmarkItem item = m_inferred;
    if (m_nameEdit) {
        const QString entered = m_nameEdit->text().trimmed();
        // Only persist `title` when the user has departed from the inferred
        // default — matches Obsidian's behavior of leaving `title` empty when
        // the display is implicit.
        if (!entered.isEmpty() && entered != inferDefaultTitle(m_inferred))
            item.title = entered;
        else
            item.title = m_inferred.title;
    }
    return item;
}

void BookmarkModal::commit()
{
    if (!m_store) return;
    const int sel = m_groupCombo ? m_groupCombo->currentIndex() : 0;
    const QStringList groupPath = (sel >= 0 && sel < m_groupPaths.size())
                                      ? m_groupPaths.at(sel)
                                      : QStringList();
    m_store->addBookmark(composedItem(), groupPath);
}

bool BookmarkModal::runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *parent)
{
    if (!store) return false;
    BookmarkModal dlg(std::move(inferred), store, parent);
    return dlg.exec() == QDialog::Accepted;
}

} // namespace Corbomite::Bookmarks
