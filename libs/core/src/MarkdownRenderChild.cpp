// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/core/MarkdownRenderChild.h"

namespace Corbomite::Core {

MarkdownRenderChild::MarkdownRenderChild() = default;
MarkdownRenderChild::~MarkdownRenderChild() = default;

void MarkdownRenderChild::setRenderedText(QString text)
{
    m_renderedText = std::move(text);
}

QString MarkdownRenderChild::renderedText() const
{
    return m_renderedText;
}

void MarkdownRenderChild::mountInto(QWidget *host)
{
    m_host = host;
}

QWidget *MarkdownRenderChild::hostWidget() const
{
    return m_host.data();
}

} // namespace Corbomite::Core
