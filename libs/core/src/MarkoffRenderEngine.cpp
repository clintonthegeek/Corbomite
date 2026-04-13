// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkoffRenderEngine.h"
#include "corbomite/core/RenderedDocument.h"
#include "corbomite/core/RenderProfile.h"
#include "corbomite/core/RenderOptions.h"

#include <QTextDocument>

namespace Corbomite {

std::unique_ptr<RenderedDocument> MarkoffRenderEngine::render(
    const QString &markdown,
    const RenderOptions &options) const
{
    // TODO(Task 7): Stub — old MD4C Renderer was deleted in Task 4.
    // This will be replaced by the new MarkoffRenderEngine that uses
    // the markoff Editor in reading mode.
    Q_UNUSED(markdown)
    Q_UNUSED(options)
    auto textDoc = std::make_unique<QTextDocument>();
    return RenderedDocument::fromQTextDocument(std::move(textDoc));
}

} // namespace Corbomite
