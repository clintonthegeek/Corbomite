// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/RenderedDocument.h"

#include <QTextBrowser>
#include <QTextDocument>

namespace Corbomite {

RenderedDocument::RenderedDocument(std::unique_ptr<QTextDocument> doc)
    : m_document(std::move(doc))
{
}

RenderedDocument::~RenderedDocument() = default;

QTextDocument *RenderedDocument::toQTextDocument() const
{
    return m_document.get();
}

QWidget *RenderedDocument::createWidget(QWidget *parent) const
{
    auto *browser = new QTextBrowser(parent);
    browser->setOpenLinks(false);
    browser->setOpenExternalLinks(false);
    browser->setReadOnly(true);

    // Clone the document content into the browser
    browser->setHtml(m_document->toHtml());
    return browser;
}

std::unique_ptr<RenderedDocument> RenderedDocument::fromQTextDocument(std::unique_ptr<QTextDocument> doc)
{
    return std::unique_ptr<RenderedDocument>(new RenderedDocument(std::move(doc)));
}

} // namespace Corbomite
