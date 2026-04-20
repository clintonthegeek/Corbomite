// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <corbomite/core/View.h>

class TstViewZoom : public QObject {
    Q_OBJECT

private:
    struct Bare : public Corbomite::View {
        Bare() : View(nullptr, nullptr) {}
        QString getViewType() const override { return {}; }
        QString getDisplayText() const override { return {}; }
    };
    struct Counting : public Corbomite::View {
        Counting() : View(nullptr, nullptr) {}
        QString getViewType() const override { return QStringLiteral("counting"); }
        QString getDisplayText() const override { return QStringLiteral("Counting"); }
        int zoomIns = 0, zoomOuts = 0, resets = 0;
        void zoomIn() override { ++zoomIns; }
        void zoomOut() override { ++zoomOuts; }
        void zoomReset() override { ++resets; }
    };

private slots:
    void defaultIsNoOp() {
        Bare v;
        v.zoomIn();
        v.zoomOut();
        v.zoomReset();
        // must not crash; no observable effect
    }
    void overridesDispatchThroughBasePointer() {
        Counting d;
        Corbomite::View *v = &d;
        v->zoomIn(); v->zoomIn(); v->zoomOut(); v->zoomReset();
        QCOMPARE(d.zoomIns, 2);
        QCOMPARE(d.zoomOuts, 1);
        QCOMPARE(d.resets, 1);
    }
};

QTEST_MAIN(TstViewZoom)
#include "tst_view_zoom.moc"
