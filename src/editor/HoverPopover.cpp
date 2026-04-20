// SPDX-License-Identifier: GPL-3.0-or-later
#include "HoverPopover.h"

#include "corbomite/core/EmbedRegistry.h"
#include "corbomite/core/MarkdownRenderChild.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/vault/Vault.h"
#include "markoff/reading/EmbedRenderer.h"
#include "markoff/reading/ReadingView.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace Corbomite {

namespace {
constexpr int kHoverDelayMs = 300;   // audit: ui-bundle.md §7
constexpr int kPopoverWidth = 380;
constexpr int kPopoverMaxHeight = 280;

/// Split a hover-link target into `(path, subpath)`. The first `#`
/// character separates the file path from the wikilink subpath; the
/// `#` is preserved on the subpath portion so callers can detect
/// `#^block` vs `#heading` discriminator. Mirrors the
/// `splitWikiEmbed` shape inside `EmbedRenderer.cpp`.
void splitTarget(const QString &raw, QString *path, QString *subpath)
{
    const int hash = raw.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        *path = raw.left(hash);
        *subpath = raw.mid(hash);
    } else {
        *path = raw;
        subpath->clear();
    }
}
} // namespace

HoverPopover::HoverPopover(QWidget *parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);
    setFixedWidth(kPopoverWidth);
    setMaximumHeight(kPopoverMaxHeight);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 70));
    setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);

    // Cluster J Phase 6 — the preview is now a real ReadingView. Math,
    // mermaid, syntax highlighting, wiki-links, and images all render
    // in-place via the same pipeline used by Reading mode.
    m_view = new Markoff::Reading::ReadingView(this);
    m_view->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_view);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(kHoverDelayMs);
    connect(&m_delayTimer, &QTimer::timeout, this, &HoverPopover::showNow);
}

HoverPopover::~HoverPopover() = default;

void HoverPopover::setVault(Vault *vault)
{
    m_vault = vault;
}

void HoverPopover::setEmbedRenderer(Markoff::Reading::EmbedRenderer *renderer)
{
    m_embedRenderer = renderer;
}

Markoff::Reading::ReadingView *HoverPopover::readingViewForTest() const
{
    return m_view;
}

void HoverPopover::scheduleShow(const QString &target, const QPoint &anchor)
{
    if (target.isEmpty()) {
        cancel();
        return;
    }
    m_pendingTarget = target;
    m_pendingAnchor = anchor;
    m_delayTimer.start();
}

void HoverPopover::cancel()
{
    m_delayTimer.stop();
    m_pendingTarget.clear();
    if (isVisible()) hide();
}

void HoverPopover::showNow()
{
    if (m_pendingTarget.isEmpty()) return;
    renderTarget(m_pendingTarget);
    move(m_pendingAnchor);
    show();
    raise();
}

void HoverPopover::renderTarget(const QString &target)
{
    if (!m_view) return;

    QString path;
    QString subpath;
    splitTarget(target, &path, &subpath);

    // Cluster J Phase 6 — preferred path: route through EmbedRenderer.
    // The renderer handles depth-guard, recursive `![[...]]` expansion,
    // image-shim conversion (`![[foo.png]]` → `![](foo.png)`), and
    // heading / `#^block` subpath slicing via MetadataCache. The
    // expanded markdown is then fed to the embedded ReadingView, which
    // owns math / mermaid / syntax-highlighting render via Phase 5's
    // built-in CodeBlockProcessorRegistry registrations.
    if (m_embedRenderer) {
        Corbomite::Core::EmbedRequest req{path, subpath, nullptr, /*depth=*/1};
        auto child = m_embedRenderer->render(req);
        if (child) {
            m_view->setPlainText(child->renderedText());
            return;
        }
    }

    // Legacy fallback — pre-Phase-6 path used when no EmbedRenderer is
    // wired (defensive; happens only in test harnesses or before the
    // first vault opens). Strips subpath; renders raw markdown only.
    if (!m_vault) {
        m_view->setPlainText(target);
        return;
    }
    auto *doc = m_vault->openDocument(path);
    if (!doc) {
        m_view->setPlainText(QStringLiteral("(unresolved: %1)").arg(target));
        return;
    }
    m_view->setPlainText(doc->markdown());
}

void HoverPopover::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    cancel();
}

void HoverPopover::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    QFrame::keyPressEvent(event);
}

} // namespace Corbomite
