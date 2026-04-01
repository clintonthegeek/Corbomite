// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_DOCUMENT_H
#define MARKOFF_DOCUMENT_H

#include <memory>
#include <QString>

namespace Markoff {

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
