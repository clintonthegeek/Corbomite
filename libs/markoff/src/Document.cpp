// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Document.h"

namespace Markoff {

struct Document::Private {
    QString source;
};

Document::Document()
    : d(std::make_unique<Private>())
{
}

Document::~Document() = default;

std::unique_ptr<Document> Document::fromMarkdown(const QString &source)
{
    auto doc = std::unique_ptr<Document>(new Document());
    doc->d->source = source;
    return doc;
}

QString Document::sourceText() const
{
    return d->source;
}

bool Document::isEmpty() const
{
    return d->source.isEmpty();
}

QString Document::extractSubpath(const QString & /*subpath*/) const
{
    return {};
}

} // namespace Markoff
