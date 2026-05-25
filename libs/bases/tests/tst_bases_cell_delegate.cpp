// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QPixmap>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStandardItemModel>
#include "corbomite/bases/BasesCellDelegate.h"
#include "corbomite/bases/BasesTreeModel.h"

using namespace Corbomite::Bases;

class TestBasesCellDelegate : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPaintsRichTypesWithoutCrash()
    {
        BasesCellDelegate d;
        QPixmap pm(80, 20);
        for (const QString &type : {QStringLiteral("Icon"), QStringLiteral("Image"),
                                    QStringLiteral("HTML"), QStringLiteral("Markdown")}) {
            QStandardItemModel m(1, 1);
            auto *it = new QStandardItem(QStringLiteral("x"));
            it->setData(type, BasesTreeModel::ValueTypeRole);
            m.setItem(0, 0, it);
            pm.fill(Qt::white);
            QPainter p(&pm);
            QStyleOptionViewItem opt; opt.rect = QRect(0, 0, 80, 20);
            d.paint(&p, opt, m.index(0, 0));   // must not crash / assert
            p.end();
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(TestBasesCellDelegate)
#include "tst_bases_cell_delegate.moc"
