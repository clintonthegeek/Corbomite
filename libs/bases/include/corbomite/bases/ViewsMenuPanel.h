// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QString>
#include <functional>

class QListWidget;

namespace Corbomite::Bases {

class BasesQuery;

/// Popup listing the query's views with Rename / Duplicate / Delete / Set
/// default actions. Mutates BasesQuery via ViewConfigOps, then calls onChanged
/// (which the owner uses to re-populate the switcher + persist). Selecting a
/// view calls onActivate(name).
class ViewsMenuPanel : public QFrame
{
    Q_OBJECT
public:
    explicit ViewsMenuPanel(QWidget *parent = nullptr);

    void setState(BasesQuery *query, const QString &activeName);
    void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }
    void setOnActivate(std::function<void(const QString &)> cb) { m_onActivate = std::move(cb); }

private:
    void rebuild();
    QString selectedName() const;

    QListWidget *m_list = nullptr;
    BasesQuery *m_query = nullptr;
    QString m_activeName;
    std::function<void()> m_onChanged;
    std::function<void(const QString &)> m_onActivate;
};

}  // namespace Corbomite::Bases
