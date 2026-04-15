// SPDX-License-Identifier: GPL-3.0-or-later
//
// MetadataParser — single-file AST walk that turns raw markdown bytes into
// a `Corbomite::CachedMetadata` + SHA-256 hash.
//
// Phase 2 ships minimal section extraction — heading + paragraph between
// headings, with the callout one-line special case. Full 11-type section
// fidelity is a follow-up; callers that need deep section info must either
// extend this parser or wait for Phase 2.5.
//
// Known Phase 2 limitations (TODOs inline below):
//  - footnote def positions via regex scan (Markoff::Document lacks offset)
//  - blockId position = marker only, not surrounding block span
//  - listItem block-anchor id = nullopt (tree-sitter exposure needed)
//  - sections coverage = heading/paragraph/callout only
//
// Offset convention (important):
//   Markoff::Document strips the frontmatter before passing bytes to
//   tree-sitter, AND removes footnote-definition lines from the body
//   before the AST walk. That means `HeadingInfo::sourceOffset` et al are
//   relative to the body-after-stripping.  We compensate for frontmatter
//   with `frontmatterOffsetShift` (= `frontmatterSpan->second`). Footnote-
//   def removal can additionally shift heading/tag/link offsets when
//   footnote-defs precede them in the source; this is a known limitation
//   that will be addressed when Markoff::Document exposes footnote-def
//   offsets natively.

#include "corbomite/storage/MetadataParser.h"

#include <functional>
#include <memory>
#include <utility>

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <markoff-parser/Document.h>
#include <markoff-parser/YamlValue.h>

#include "corbomite/storage/LinkResolver.h"

namespace Corbomite {

namespace {

// ---------------------------------------------------------------------------
// OffsetToPosConverter — stateful (line, col, offset) computer that assumes
// monotonically-increasing offset queries for O(n) total scan cost. Falls
// back to a from-zero scan if a later query goes backwards.
// ---------------------------------------------------------------------------

struct OffsetToPosConverter
{
    const QString &text;
    int lastOffset = 0;
    int lastLine = 0;
    int lastLineStart = 0; // offset of the first char on `lastLine`

    explicit OffsetToPosConverter(const QString &t) : text(t) {}

    Pos at(int offset)
    {
        // Clamp to [0, text.size()].
        if (offset < 0)
            offset = 0;
        if (offset > text.size())
            offset = text.size();

        if (offset < lastOffset) {
            // Non-monotonic call — reset to zero and rescan.
            lastOffset = 0;
            lastLine = 0;
            lastLineStart = 0;
        }

        // Advance from lastOffset to offset, counting newlines.
        for (int i = lastOffset; i < offset; ++i) {
            if (text.at(i) == QLatin1Char('\n')) {
                ++lastLine;
                lastLineStart = i + 1;
            }
        }
        lastOffset = offset;

        Pos p;
        p.line = lastLine;
        p.col = offset - lastLineStart;
        p.offset = offset;
        return p;
    }
};

Position spanToPosition(OffsetToPosConverter &conv, int startOffset, int endOffset)
{
    // Using a single converter for both endpoints is fine because
    // startOffset <= endOffset for well-formed spans, which keeps the
    // converter monotonic across the pair.
    Position pos;
    pos.start = conv.at(startOffset);
    pos.end = conv.at(endOffset);
    return pos;
}

// ---------------------------------------------------------------------------
// Link-literal length — used to compute `end` offsets for link positions.
// ---------------------------------------------------------------------------

int linkLiteralLength(const Markoff::LinkInfo &info)
{
    switch (info.type) {
    case Markoff::LinkInfo::Wiki: {
        // [[target]] or [[target|display]]
        int base = 4 + info.target.size(); // "[[" + target + "]]"
        if (!info.displayText.isEmpty() && info.displayText != info.target)
            base += 1 + info.displayText.size(); // "|display"
        return base;
    }
    case Markoff::LinkInfo::Embed: {
        // ![[target]] or ![[target|display]]
        int base = 5 + info.target.size(); // "![[" + target + "]]"
        if (!info.displayText.isEmpty() && info.displayText != info.target)
            base += 1 + info.displayText.size();
        return base;
    }
    case Markoff::LinkInfo::Standard: {
        // [display](target)
        return 4 + info.displayText.size() + info.target.size();
    }
    case Markoff::LinkInfo::Image: {
        // ![display](target)
        return 5 + info.displayText.size() + info.target.size();
    }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// YamlValue -> QJsonValue
// ---------------------------------------------------------------------------

QJsonValue yamlValueToJson(const Markoff::YamlValue &v)
{
    using K = Markoff::YamlValue::Kind;
    switch (v.kind()) {
    case K::Null:
        return QJsonValue(QJsonValue::Null);
    case K::Bool:
        return QJsonValue(v.asBool());
    case K::Int:
        // QJsonValue has no 64-bit int constructor; fall through as double.
        return QJsonValue(static_cast<double>(v.asInt()));
    case K::Double:
        return QJsonValue(v.asDouble());
    case K::String:
        return QJsonValue(v.asString());
    case K::Seq: {
        QJsonArray arr;
        const int n = v.size();
        for (int i = 0; i < n; ++i)
            arr.append(yamlValueToJson(v.at(i)));
        return arr;
    }
    case K::Map: {
        QJsonObject obj;
        v.forEach([&obj](const QString &key, const Markoff::YamlValue &val) {
            obj.insert(key, yamlValueToJson(val));
        });
        return obj;
    }
    }
    return QJsonValue(QJsonValue::Null);
}

// ---------------------------------------------------------------------------
// Frontmatter-link walker: collect wiki-links & markdown-links embedded in
// any string-typed leaf of the parsed frontmatter tree. Dotted-key convention:
// nested objects concat `parent.child`; array elements concat `parent.index`.
// ---------------------------------------------------------------------------

void collectFrontmatterLinks(const QJsonValue &v,
                             const QString &keyPath,
                             QVector<FrontmatterLinkCache> &out)
{
    if (v.isObject()) {
        const QJsonObject obj = v.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QString sub = keyPath.isEmpty() ? it.key()
                                                  : keyPath + QLatin1Char('.') + it.key();
            collectFrontmatterLinks(it.value(), sub, out);
        }
        return;
    }
    if (v.isArray()) {
        const QJsonArray arr = v.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            const QString sub = keyPath.isEmpty()
                                    ? QString::number(i)
                                    : keyPath + QLatin1Char('.') + QString::number(i);
            collectFrontmatterLinks(arr.at(i), sub, out);
        }
        return;
    }
    if (!v.isString())
        return;

    const QString s = v.toString();

    // Wiki-link: [[target]] or [[target|display]]
    static const QRegularExpression wikiRe(
        QStringLiteral(R"(\[\[([^|\]]+)(?:\|([^\]]*))?\]\])"));
    {
        auto m = wikiRe.match(s);
        if (m.hasMatch()) {
            FrontmatterLinkCache fml;
            fml.link = m.captured(1);
            fml.original = m.captured(0);
            const QString disp = m.captured(2);
            if (!disp.isEmpty())
                fml.displayText = disp;
            fml.key = keyPath;
            out.append(fml);
            return;
        }
    }

    // Markdown-link: [display](target)
    static const QRegularExpression mdRe(
        QStringLiteral(R"(\[([^\]]*)\]\(([^)]+)\))"));
    {
        auto m = mdRe.match(s);
        if (m.hasMatch()) {
            FrontmatterLinkCache fml;
            fml.link = m.captured(2);
            fml.original = m.captured(0);
            const QString disp = m.captured(1);
            if (!disp.isEmpty())
                fml.displayText = disp;
            fml.key = keyPath;
            out.append(fml);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Enum mapper for SectionCache::Type.
// ---------------------------------------------------------------------------

SectionCache makeHeadingSection(OffsetToPosConverter &conv, int start, int end)
{
    SectionCache s;
    s.type = SectionCache::Type::Heading;
    s.position = spanToPosition(conv, start, end);
    return s;
}

SectionCache makeParagraphSection(OffsetToPosConverter &conv, int start, int end)
{
    SectionCache s;
    s.type = SectionCache::Type::Paragraph;
    s.position = spanToPosition(conv, start, end);
    return s;
}

SectionCache makeCalloutSection(OffsetToPosConverter &conv, int start, int end)
{
    SectionCache s;
    s.type = SectionCache::Type::Callout;
    s.position = spanToPosition(conv, start, end);
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// MetadataParser::parse
// ---------------------------------------------------------------------------

ParsedNote MetadataParser::parse(const QByteArray &content,
                                 const QString &path,
                                 const LinkResolver &resolver)
{
    // (a) SHA-256 content hash (64-char lowercase hex).
    const QByteArray hashBytes =
        QCryptographicHash::hash(content, QCryptographicHash::Sha256);
    const QString hashHex = QString::fromLatin1(hashBytes.toHex());

    CachedMetadata cache;

    // (b) Parse the document.
    const QString text = QString::fromUtf8(content);
    std::unique_ptr<Markoff::Document> doc = Markoff::Document::fromMarkdown(text);
    if (!doc)
        return ParsedNote{hashHex, std::move(cache)};

    OffsetToPosConverter conv(text);

    // Compute frontmatter-offset shift. Markoff's tree-sitter offsets are
    // relative to the body (frontmatter stripped), so we add this shift
    // to get offsets into `text`.
    int frontmatterOffsetShift = 0;
    const auto fmSpan = doc->frontmatterSpan();
    if (fmSpan.has_value())
        frontmatterOffsetShift = fmSpan->second;

    // (h) Frontmatter (parse first so frontmatter-tags step (g) can merge).
    QJsonObject frontmatterObj;
    bool haveFrontmatter = false;
    Markoff::YamlValue fm = doc->parsedFrontmatter();
    if (fm.isMap()) {
        QJsonValue v = yamlValueToJson(fm);
        if (v.isObject()) {
            frontmatterObj = v.toObject();
            haveFrontmatter = true;
        }
    } else if (fmSpan.has_value()) {
        // Unusual: frontmatter present but not a mapping. Store empty object.
        haveFrontmatter = true;
    }
    if (haveFrontmatter)
        cache.frontmatter = frontmatterObj;

    if (fmSpan.has_value()) {
        int s = fmSpan->first;
        int e = fmSpan->second;
        if (s < 0) s = 0;
        if (e > text.size()) e = text.size();
        // Use a separate converter for frontmatter position to avoid
        // breaking monotonicity of the main converter (which will shortly
        // process offsets past the frontmatter).
        OffsetToPosConverter fmConv(text);
        cache.frontmatterPosition = spanToPosition(fmConv, s, e);
    }

    // (e) Headings.
    const QList<Markoff::HeadingInfo> headings = doc->headings();
    for (const Markoff::HeadingInfo &h : headings) {
        const int absOffset = h.sourceOffset + frontmatterOffsetShift;
        // Scan forward from absOffset to find the end of the heading line.
        int end = absOffset;
        while (end < text.size() && text.at(end) != QLatin1Char('\n'))
            ++end;

        HeadingCache hc;
        hc.heading = h.text;
        hc.level = h.level;
        hc.position = spanToPosition(conv, absOffset, end);
        if (!cache.headings.has_value())
            cache.headings = QVector<HeadingCache>{};
        cache.headings->append(hc);
    }

    // (f) Links + embeds.
    const QList<Markoff::LinkInfo> links = doc->links();
    for (const Markoff::LinkInfo &info : links) {
        ResolvedLink resolved = resolver.resolve(path, info.target);

        QString storedLink;
        if (resolved.resolved)
            storedLink = resolved.path + resolved.subpath;
        else
            storedLink = info.target; // pass-through for unresolved

        QString original;
        if (!info.displayText.isEmpty() && info.displayText != info.target)
            original = info.target + QLatin1Char('|') + info.displayText;
        else
            original = info.target;

        std::optional<QString> displayText;
        if (!info.displayText.isEmpty() && info.displayText != info.target)
            displayText = info.displayText;

        const int absOffset = info.sourceOffset + frontmatterOffsetShift;
        const int len = linkLiteralLength(info);

        LinkCache lc;
        lc.link = storedLink;
        lc.original = original;
        lc.displayText = displayText;
        lc.position = spanToPosition(conv, absOffset, absOffset + len);

        switch (info.type) {
        case Markoff::LinkInfo::Wiki:
        case Markoff::LinkInfo::Standard:
            if (!cache.links.has_value())
                cache.links = QVector<LinkCache>{};
            cache.links->append(lc);
            break;
        case Markoff::LinkInfo::Embed:
        case Markoff::LinkInfo::Image:
            if (!cache.embeds.has_value())
                cache.embeds = QVector<LinkCache>{};
            cache.embeds->append(lc);
            break;
        }
    }

    // (g) Inline tags.
    const QList<Markoff::TagInfo> tagInfos = doc->tags();
    for (const Markoff::TagInfo &t : tagInfos) {
        const int absOffset = t.sourceOffset + frontmatterOffsetShift;
        TagCache tc;
        tc.tag = QLatin1Char('#') + t.name;
        tc.position =
            spanToPosition(conv, absOffset, absOffset + 1 + t.name.size());
        if (!cache.tags.has_value())
            cache.tags = QVector<TagCache>{};
        cache.tags->append(tc);
    }

    // (g cont'd) Frontmatter-merged tags: tags (array) + tag (singular).
    if (haveFrontmatter) {
        Position fmPos;
        if (cache.frontmatterPosition.has_value())
            fmPos = *cache.frontmatterPosition;

        auto pushFmTag = [&](const QString &raw) {
            QString name = raw.trimmed();
            if (name.isEmpty())
                return;
            TagCache tc;
            tc.tag = name.startsWith(QLatin1Char('#')) ? name
                                                       : QLatin1Char('#') + name;
            tc.position = fmPos;
            if (!cache.tags.has_value())
                cache.tags = QVector<TagCache>{};
            cache.tags->append(tc);
        };

        const QJsonValue tagsVal = frontmatterObj.value(QStringLiteral("tags"));
        if (tagsVal.isArray()) {
            const QJsonArray arr = tagsVal.toArray();
            for (const QJsonValue &v : arr) {
                if (v.isString())
                    pushFmTag(v.toString());
            }
        } else if (tagsVal.isString()) {
            // Space/comma separated string — treat the whole thing as one tag
            // per Obsidian's permissive handling.
            pushFmTag(tagsVal.toString());
        }

        const QJsonValue tagVal = frontmatterObj.value(QStringLiteral("tag"));
        if (tagVal.isString())
            pushFmTag(tagVal.toString());
        else if (tagVal.isArray()) {
            const QJsonArray arr = tagVal.toArray();
            for (const QJsonValue &v : arr) {
                if (v.isString())
                    pushFmTag(v.toString());
            }
        }
    }

    // (i) Frontmatter links (dotted-key walk).
    if (haveFrontmatter) {
        QVector<FrontmatterLinkCache> fmLinks;
        for (auto it = frontmatterObj.constBegin(); it != frontmatterObj.constEnd();
             ++it) {
            collectFrontmatterLinks(it.value(), it.key(), fmLinks);
        }
        if (!fmLinks.isEmpty())
            cache.frontmatterLinks = fmLinks;
    }

    // (j) Footnote *definitions*. Markoff strips these from the body before
    // tree-sitter, and FootnoteInfo does not expose a source offset. Workaround:
    // regex-scan `doc->markdownContent()` (which is body-after-frontmatter-strip
    // but BEFORE footnote-def removal) for `[^id]:` at start-of-line.
    // TODO: move to tree-sitter exposure when Markoff::Document exposes footnote
    // definition offsets.
    const QString body = doc->markdownContent();
    {
        const QList<Markoff::FootnoteInfo> footnotes = doc->footnotes();
        if (!footnotes.isEmpty()) {
            QHash<QString, Position> defPos;
            static const QRegularExpression defRe(
                QStringLiteral(R"(^\[\^([^\]]+)\]:[^\n]*)"),
                QRegularExpression::MultilineOption);
            auto it = defRe.globalMatch(body);
            // Reuse a body-local converter since positions need to be
            // relative to `text` (body is text.mid(frontmatterOffsetShift)).
            OffsetToPosConverter bodyConv(text);
            while (it.hasNext()) {
                auto m = it.next();
                const QString label = m.captured(1);
                const int absStart = m.capturedStart() + frontmatterOffsetShift;
                const int absEnd = m.capturedEnd() + frontmatterOffsetShift;
                if (!defPos.contains(label))
                    defPos.insert(label, spanToPosition(bodyConv, absStart, absEnd));
            }

            QVector<FootnoteCache> fns;
            for (const Markoff::FootnoteInfo &fi : footnotes) {
                FootnoteCache fc;
                fc.id = fi.label;
                if (defPos.contains(fi.label))
                    fc.position = defPos.value(fi.label);
                fns.append(fc);
            }
            if (!fns.isEmpty())
                cache.footnotes = fns;
        }
    }

    // (k) Footnote *refs*. Scan body for `[^id]` not at line-start-followed-by-`:`.
    {
        static const QRegularExpression refRe(QStringLiteral(R"(\[\^([^\]]+)\])"));
        OffsetToPosConverter refConv(text);
        QVector<FootnoteCache> refs;
        auto it = refRe.globalMatch(body);
        while (it.hasNext()) {
            auto m = it.next();
            const int matchStart = m.capturedStart();
            const int matchEnd = m.capturedEnd();

            // At line start?
            bool atLineStart = (matchStart == 0) ||
                               body.at(matchStart - 1) == QLatin1Char('\n');
            bool followedByColon =
                (matchEnd < body.size()) && body.at(matchEnd) == QLatin1Char(':');
            if (atLineStart && followedByColon)
                continue; // it's a definition, not a ref

            const int absStart = matchStart + frontmatterOffsetShift;
            const int absEnd = matchEnd + frontmatterOffsetShift;

            FootnoteCache fc;
            fc.id = m.captured(1);
            fc.position = spanToPosition(refConv, absStart, absEnd);
            refs.append(fc);
        }
        if (!refs.isEmpty())
            cache.footnoteRefs = refs;
    }

    // (l) Block anchors: `^blockid` at end-of-line. Phase 2 uses marker span.
    // TODO: expand position to surrounding block via AST when available.
    {
        static const QRegularExpression blockRe(
            QStringLiteral(R"(\^([A-Za-z0-9\-]+)(?=\s*$))"),
            QRegularExpression::MultilineOption);
        OffsetToPosConverter blockConv(text);
        QHash<QString, BlockCache> blocks;
        auto it = blockRe.globalMatch(body);
        while (it.hasNext()) {
            auto m = it.next();
            const int matchStart = m.capturedStart();
            int lineEnd = matchStart;
            while (lineEnd < body.size() && body.at(lineEnd) != QLatin1Char('\n'))
                ++lineEnd;
            const int absStart = matchStart + frontmatterOffsetShift;
            const int absEnd = lineEnd + frontmatterOffsetShift;

            BlockCache bc;
            bc.id = m.captured(1);
            bc.position = spanToPosition(blockConv, absStart, absEnd);
            blocks.insert(bc.id, bc);
        }
        if (!blocks.isEmpty())
            cache.blocks = blocks;
    }

    // (m) Sections — minimal Phase 2 extraction: heading + paragraph (or
    // callout) between headings.
    // TODO: full-fidelity section extraction via tree-sitter.
    {
        QVector<SectionCache> sections;
        // Build an array of heading absolute spans.
        struct HSpan
        {
            int absStart;
            int absEnd;
        };
        QVector<HSpan> headingSpans;
        headingSpans.reserve(headings.size());
        for (const Markoff::HeadingInfo &h : headings) {
            const int absOffset = h.sourceOffset + frontmatterOffsetShift;
            int end = absOffset;
            while (end < text.size() && text.at(end) != QLatin1Char('\n'))
                ++end;
            headingSpans.append({absOffset, end});
        }

        OffsetToPosConverter secConv(text);

        // Also: if there's content before the first heading, emit a
        // paragraph/callout section for it (if non-whitespace).
        const int bodyStart = frontmatterOffsetShift;
        const int bodyEnd = text.size();

        auto paragraphIntervalHasContent = [&](int s, int e) {
            // Advance past leading whitespace/newlines.
            while (s < e && text.at(s).isSpace())
                ++s;
            return s < e;
        };

        auto findLeadingNonSpace = [&](int s, int e) {
            while (s < e && text.at(s).isSpace())
                ++s;
            return s;
        };

        auto findTrailingNonSpace = [&](int s, int e) {
            int idx = e;
            while (idx > s && text.at(idx - 1).isSpace())
                --idx;
            return idx;
        };

        auto addParagraphOrCallout = [&](int s, int e) {
            int cs = findLeadingNonSpace(s, e);
            int ce = findTrailingNonSpace(cs, e);
            if (cs >= ce)
                return;
            // Callout?
            const bool isCallout =
                (cs + 3 <= text.size()) && text.at(cs) == QLatin1Char('>') &&
                ((cs + 1 < text.size() && text.at(cs + 1) == QLatin1Char(' ') &&
                  cs + 3 < text.size() && text.at(cs + 2) == QLatin1Char('[') &&
                  text.at(cs + 3) == QLatin1Char('!')) ||
                 (cs + 2 < text.size() && text.at(cs + 1) == QLatin1Char('[') &&
                  text.at(cs + 2) == QLatin1Char('!')));
            if (isCallout)
                sections.append(makeCalloutSection(secConv, cs, ce));
            else
                sections.append(makeParagraphSection(secConv, cs, ce));
        };

        if (!headingSpans.isEmpty()) {
            // Pre-first-heading content.
            if (paragraphIntervalHasContent(bodyStart, headingSpans.first().absStart))
                addParagraphOrCallout(bodyStart, headingSpans.first().absStart);

            for (int i = 0; i < headingSpans.size(); ++i) {
                // Heading section spans the heading line itself.
                sections.append(makeHeadingSection(
                    secConv, headingSpans[i].absStart, headingSpans[i].absEnd));

                // Gap between this heading line and the next (or EOF).
                const int gapStart = headingSpans[i].absEnd;
                const int gapEnd = (i + 1 < headingSpans.size())
                                       ? headingSpans[i + 1].absStart
                                       : bodyEnd;
                if (paragraphIntervalHasContent(gapStart, gapEnd))
                    addParagraphOrCallout(gapStart, gapEnd);
            }
        } else {
            // No headings — whole body is one paragraph/callout if non-empty.
            if (paragraphIntervalHasContent(bodyStart, bodyEnd))
                addParagraphOrCallout(bodyStart, bodyEnd);
        }

        if (!sections.isEmpty())
            cache.sections = sections;
    }

    // (n) List items.
    // Regex over the BODY (line-based). Nesting via an indent-stack.
    {
        // NOTE: `\s*` would eat the preceding `\n` on blank lines (since
        // `\s` matches newline), which would shift `capturedStart` onto
        // the blank-line newline rather than the list bullet. Restrict the
        // indent class to horizontal whitespace (space and tab) only.
        static const QRegularExpression listRe(
            QStringLiteral(R"(^([ \t]*)([-*+]|\d+\.)[ \t]+(?:\[([ xX\-])\][ \t]+)?(.*)$)"),
            QRegularExpression::MultilineOption);

        QVector<ListItemCache> items;
        QVector<QPair<int, int>> stack; // (indent, index-into-items)
        OffsetToPosConverter liConv(text);

        auto it = listRe.globalMatch(body);
        while (it.hasNext()) {
            auto m = it.next();
            const int indent = m.captured(1).size();
            const QString task = m.captured(3);
            const int lineStart = m.capturedStart();
            int lineEnd = m.capturedEnd();

            const int absStart = lineStart + frontmatterOffsetShift;
            const int absEnd = lineEnd + frontmatterOffsetShift;

            // Pop stack while top indent >= this indent.
            while (!stack.isEmpty() && stack.back().first >= indent)
                stack.pop_back();

            const int parent = stack.isEmpty() ? -1 : stack.back().second;

            ListItemCache li;
            li.position = spanToPosition(liConv, absStart, absEnd);
            li.parent = parent;
            if (!task.isEmpty())
                li.task = task;
            // TODO: populate `id` when list-item block-anchor syntax is
            // exposed by Markoff::Document (Phase 2.5 follow-up).
            items.append(li);

            stack.append(qMakePair(indent, items.size() - 1));
        }
        if (!items.isEmpty())
            cache.listItems = items;
    }

    // (o) Return.
    return ParsedNote{hashHex, std::move(cache)};
}

} // namespace Corbomite
