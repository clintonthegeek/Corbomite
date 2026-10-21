// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/canvas/CanvasLinkRewriter.h"

#include "corbomite/canvas/CanvasDocument.h"
#include "corbomite/canvas/CanvasDocumentRegistry.h"
#include "corbomite/canvas/CanvasTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace Corbomite::Canvas {

namespace {

// Rewrite `[[target#sub|alias]]` / `![[…]]` targets inside a text card.
QString remapWikilinks(const QString &text,
                       const Corbomite::LinkRewriter::TargetMapper &mapTarget,
                       bool &changed)
{
    static const QRegularExpression re(
        QStringLiteral(R"((!?)\[\[([^\]|#]+)((?:#[^\]|]*)?(?:\|[^\]]*)?)\]\])"));
    QString out;
    int last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString target = m.captured(2);
        const std::optional<QString> mapped = mapTarget(target);
        if (!mapped.has_value() || *mapped == target) continue;
        out += text.mid(last, m.capturedStart() - last);
        out += m.captured(1) + QStringLiteral("[[") + *mapped + m.captured(3)
               + QStringLiteral("]]");
        last = m.capturedEnd();
        changed = true;
    }
    if (!changed) return text;
    out += text.mid(last);
    return out;
}

// Rewrite `[label](url#sub)` / `![label](url)` targets inside a text card.
QString remapMarkdownLinks(const QString &text,
                           const Corbomite::LinkRewriter::TargetMapper &mapTarget,
                           bool &changed)
{
    static const QRegularExpression re(
        QStringLiteral(R"((!?)\[([^\]]*)\]\(([^)\s#]+)(#[^)\s]*)?\))"));
    QString out;
    int last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString url = m.captured(3);
        const std::optional<QString> mapped = mapTarget(url);
        if (!mapped.has_value() || *mapped == url) continue;
        out += text.mid(last, m.capturedStart() - last);
        out += m.captured(1) + QStringLiteral("[") + m.captured(2)
               + QStringLiteral("](") + *mapped + m.captured(4)
               + QStringLiteral(")");
        last = m.capturedEnd();
        changed = true;
    }
    if (!changed) return text;
    out += text.mid(last);
    return out;
}

QString remapTextLinks(const QString &text,
                       const Corbomite::LinkRewriter::TargetMapper &mapTarget,
                       bool &changed)
{
    QString out = remapWikilinks(text, mapTarget, changed);
    bool mdChanged = false;
    out = remapMarkdownLinks(out, mapTarget, mdChanged);
    changed = changed || mdChanged;
    return out;
}

// Apply every structural link rewrite to `doc`. Returns true when any node
// changed.
bool applyUpdates(CanvasDocument &doc,
                  const Corbomite::LinkRewriter::TargetMapper &mapTarget)
{
    bool changed = false;
    const QVector<CanvasNode> nodes = doc.nodes();
    for (const CanvasNode &n : nodes) {
        CanvasNode updated = n;
        bool nodeChanged = false;
        if (n.type == NodeType::File && !n.file.isEmpty()) {
            const std::optional<QString> mapped = mapTarget(n.file);
            if (mapped.has_value() && *mapped != n.file) {
                updated.file = *mapped;
                nodeChanged = true;
            }
        } else if (n.type == NodeType::Group && !n.background.isEmpty()) {
            const std::optional<QString> mapped = mapTarget(n.background);
            if (mapped.has_value() && *mapped != n.background) {
                updated.background = *mapped;
                nodeChanged = true;
            }
        } else if (n.type == NodeType::Text && !n.text.isEmpty()) {
            bool textChanged = false;
            const QString rewritten = remapTextLinks(n.text, mapTarget, textChanged);
            if (textChanged) {
                updated.text = rewritten;
                nodeChanged = true;
            }
        }
        if (nodeChanged) {
            doc.updateNode(updated);
            changed = true;
        }
    }
    return changed;
}

}  // namespace

QString CanvasLinkRewriter::extension() const
{
    return QStringLiteral("canvas");
}

std::optional<QByteArray> CanvasLinkRewriter::rewrite(
    const QString &path, const QByteArray &bytes, const TargetMapper &mapTarget)
{
    // Open canvas: mutate the live document and let its view persist it.
    // Writing disk behind the open instance would desync it (single writer).
    if (CanvasDocument *live = CanvasDocumentRegistry::instance().documentFor(path)) {
        applyUpdates(*live, mapTarget);
        return std::nullopt;
    }

    const QJsonDocument json = QJsonDocument::fromJson(bytes);
    if (!json.isObject()) return std::nullopt;

    CanvasDocument doc;
    if (!doc.loadFromJson(json.object())) return std::nullopt;
    if (!applyUpdates(doc, mapTarget)) return std::nullopt;
    return doc.serializeObsidianStyle();
}

} // namespace Corbomite::Canvas
