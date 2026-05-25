// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_MARKDOWNRENDERCHILD_H
#define CORBOMITE_CORE_MARKDOWNRENDERCHILD_H

#include "corbomite/core/Component.h"

// TODO(port-foundation-exploration): Markoff::MarkdownRenderChild was retired
// with the old leaves. Inheritance + override removed; setRenderedText /
// renderedText (inherited from Markoff base) are stubbed locally so the
// class compiles standalone.
// #include <markoff/MarkdownRenderChild.h>

#include <QPointer>
#include <QString>
#include <QWidget>

namespace Corbomite::Core {

/// Lifecycle-tied widget/scene-node subtree produced by post-processors,
/// code-block processors, and embed renderers. Auto-unloads when its
/// containing section is recycled by ReadingView's SectionRecyclePool.
class MarkdownRenderChild : public Corbomite::Component
{
public:
    MarkdownRenderChild();
    ~MarkdownRenderChild() override;

    /// Attach this child's widget subtree to `host`.
    void mountInto(QWidget *host);
    QWidget *hostWidget() const;

    // TODO(port-foundation-exploration): formerly inherited from
    // Markoff::MarkdownRenderChild. Stubbed locally pending the new
    // Markoff::MarkdownRenderChild abstract (E3 work).
    void setRenderedText(const QString &t) { m_renderedText = t; }
    QString renderedText() const { return m_renderedText; }

private:
    QPointer<QWidget> m_host;
    QString m_renderedText;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_MARKDOWNRENDERCHILD_H
