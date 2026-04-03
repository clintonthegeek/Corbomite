// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_DOCUMENT_H
#define MARKOFF_DOCUMENT_H

#include <memory>
#include <QString>
#include <QList>

namespace Markoff {

struct HeadingInfo {
    int level;
    QString text;
    int sourceOffset;
};

struct LinkInfo {
    enum Type { Standard, Wiki, Image, Embed };
    Type type;
    QString target;
    QString displayText;
    int sourceOffset;
};

struct TagInfo {
    QString name;
    int sourceOffset;
};

struct FootnoteInfo {
    int number;
    QString label;
    QString content;
};

class Document
{
public:
    ~Document();

    static std::unique_ptr<Document> fromMarkdown(const QString &source);

    QString sourceText() const;
    bool isEmpty() const;
    QString extractSubpath(const QString &subpath) const;
    QString frontmatter() const;

    // Returns the markdown content without frontmatter
    QString markdownContent() const;

    // Footnote access for the renderer
    int footnoteCount() const;
    QString footnoteContent(int number) const;  // 1-based

    // Query API
    QList<HeadingInfo> headings() const;
    QList<LinkInfo> links() const;
    QList<LinkInfo> wikiLinks() const;
    QList<TagInfo> tags() const;
    QList<FootnoteInfo> footnotes() const;
    int wordCount() const;
    int characterCount() const;

private:
    Document();

    struct Private;
    std::unique_ptr<Private> d;

    // Internal accessor for library components (Renderer, Editor)
    // Defined in Document.cpp, declared in DocumentBuilder_p.h
    friend struct DocumentBlockAccessor;
};

} // namespace Markoff

#endif // MARKOFF_DOCUMENT_H
