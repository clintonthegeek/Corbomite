// SPDX-License-Identifier: GPL-3.0-or-later
//
// CanvasLinkRewriter. A vault rename must update the
// structural link references inside a `.canvas` (file-node `file`, group
// `background`, and text-card wikilinks / markdown links) through
// CanvasDocument, preserving unknown fields, and must mutate the open live
// document instead of writing a second copy to disk.
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <optional>

#include "canvas/CanvasDocument.h"
#include "canvas/CanvasDocumentRegistry.h"
#include "canvas/CanvasLinkRewriter.h"

using Canvas::CanvasDocument;
using Canvas::CanvasDocumentRegistry;
using Canvas::CanvasLinkRewriter;
using Corbomite::LinkRewriter;

namespace {

LinkRewriter::TargetMapper oldToNew()
{
    return [](const QString &target) -> std::optional<QString> {
        if (target == QStringLiteral("notes/Old.md"))
            return QStringLiteral("notes/New.md");
        return std::nullopt;
    };
}

const QByteArray kFileNodeCanvas =
    R"({"custom":{"keep":1},"nodes":[{"id":"n1","type":"file","file":"notes/Old.md",)"
    R"("subpath":"#H","x":0,"y":0,"width":250,"height":60,"customNode":true}],"edges":[]})";

}  // namespace

class TestCanvasLinkRewriter : public QObject
{
    Q_OBJECT
private slots:
    void rewritesFileNodeAndPreservesUnknown();
    void rewritesGroupBackground();
    void rewritesTextCardLinks();
    void noMatchReturnsNullopt();
    void openDocumentIsMutatedInPlace();
};

void TestCanvasLinkRewriter::rewritesFileNodeAndPreservesUnknown()
{
    CanvasLinkRewriter rewriter;
    const std::optional<QByteArray> out =
        rewriter.rewrite(QStringLiteral("board.canvas"), kFileNodeCanvas, oldToNew());
    QVERIFY(out.has_value());

    const QJsonObject o = QJsonDocument::fromJson(*out).object();
    // Unknown top-level and unknown node fields survive.
    QCOMPARE(o.value(QStringLiteral("custom")).toObject()
                 .value(QStringLiteral("keep")).toInt(), 1);
    const QJsonObject n =
        o.value(QStringLiteral("nodes")).toArray().at(0).toObject();
    QCOMPARE(n.value(QStringLiteral("file")).toString(),
             QStringLiteral("notes/New.md"));
    // The subpath is a separate field and is untouched.
    QCOMPARE(n.value(QStringLiteral("subpath")).toString(), QStringLiteral("#H"));
    QVERIFY(n.value(QStringLiteral("customNode")).toBool());
}

void TestCanvasLinkRewriter::rewritesGroupBackground()
{
    const QByteArray src =
        R"({"nodes":[{"id":"g1","type":"group","background":"notes/Old.md",)"
        R"("label":"G","x":0,"y":0,"width":100,"height":100}],"edges":[]})";
    CanvasLinkRewriter rewriter;
    const std::optional<QByteArray> out =
        rewriter.rewrite(QStringLiteral("board.canvas"), src, oldToNew());
    QVERIFY(out.has_value());
    const QJsonObject n = QJsonDocument::fromJson(*out).object()
                              .value(QStringLiteral("nodes")).toArray().at(0).toObject();
    QCOMPARE(n.value(QStringLiteral("background")).toString(),
             QStringLiteral("notes/New.md"));
}

void TestCanvasLinkRewriter::rewritesTextCardLinks()
{
    const QByteArray src =
        R"({"nodes":[{"id":"t1","type":"text","text":"See [[notes/Old.md]] and )"
        R"([x](notes/Old.md) and ![[keep.png]]","x":0,"y":0,"width":250,"height":60}],)"
        R"("edges":[]})";
    CanvasLinkRewriter rewriter;
    const std::optional<QByteArray> out =
        rewriter.rewrite(QStringLiteral("board.canvas"), src, oldToNew());
    QVERIFY(out.has_value());
    const QString text = QJsonDocument::fromJson(*out).object()
                             .value(QStringLiteral("nodes")).toArray().at(0).toObject()
                             .value(QStringLiteral("text")).toString();
    QCOMPARE(text, QStringLiteral(
                       "See [[notes/New.md]] and [x](notes/New.md) and ![[keep.png]]"));
}

void TestCanvasLinkRewriter::noMatchReturnsNullopt()
{
    CanvasLinkRewriter rewriter;
    const std::optional<QByteArray> out =
        rewriter.rewrite(QStringLiteral("board.canvas"), kFileNodeCanvas,
                         [](const QString &) -> std::optional<QString> {
                             return std::nullopt;
                         });
    QVERIFY(!out.has_value());
}

void TestCanvasLinkRewriter::openDocumentIsMutatedInPlace()
{
    CanvasDocument live;
    QVERIFY(live.loadFromJson(
        QJsonDocument::fromJson(kFileNodeCanvas).object()));
    CanvasDocumentRegistry::instance().attach(QStringLiteral("board.canvas"), &live);

    CanvasLinkRewriter rewriter;
    const std::optional<QByteArray> out =
        rewriter.rewrite(QStringLiteral("board.canvas"), kFileNodeCanvas, oldToNew());

    // Single writer: no bytes returned, but the live document is updated.
    QVERIFY(!out.has_value());
    QCOMPARE(live.node(QStringLiteral("n1")).file, QStringLiteral("notes/New.md"));

    CanvasDocumentRegistry::instance().detach(QStringLiteral("board.canvas"), &live);
}

QTEST_MAIN(TestCanvasLinkRewriter)
#include "tst_canvas_link_rewriter.moc"
