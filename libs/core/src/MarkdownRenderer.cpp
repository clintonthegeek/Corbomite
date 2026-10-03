// libs/core/src/MarkdownRenderer.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Release hygiene C.1 — this file USED to be ~602 lines, of which 594 were
// inside an `#if 0` block holding a pre-contract-v1 Markdown->HTML renderer.
// That body is deleted, not merely disabled, because a commented-out
// implementation is indistinguishable from a specification: it never gets
// updated, never gets removed on its own, and every reader has to re-derive
// whether it is live. See docs/decisions-archive.md for the purge.
//
// WHAT SURVIVES, AND WHY IT IS NOT DEAD: `renderToHtml` is declared outside
// the old `#if 0` and is genuinely called — `RegexRenderEngine::renderHtml`
// holds a `MarkdownRenderer` by value and calls it
// (libs/core/src/RegexRenderEngine.cpp:19), and `tst_markdownrenderer` links
// against it. So the class stays; only its dead body goes. The stub returns an
// empty string, which is why that test asserts nothing meaningful — it is one
// of the five QSKIP tests filed in the punch list.
#include "corbomite/core/MarkdownRenderer.h"

namespace Corbomite {

QString MarkdownRenderer::renderToHtml(const QString & /*markdown*/) const
{
    return {};
}

} // namespace Corbomite
