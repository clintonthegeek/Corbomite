// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/ReadingView.h"
#include "markoff/Document.h"
#include "markoff/Renderer.h"
#include "markoff/RenderSettings.h"
#include "markoff/ResourceProvider.h"
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QUrl>
#include <QTextCursor>
#include <cmath>

namespace Markoff {

struct ReadingView::Private {
    QTextBrowser *browser = nullptr;
    Markoff::Renderer renderer;
    Theme theme;
    RenderSettings renderSettings;
    ResourceProvider *resourceProvider = nullptr;
    std::unique_ptr<Document> document;
};

ReadingView::ReadingView(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->browser = new QTextBrowser(this);
    d->browser->setOpenLinks(false);
    d->browser->setOpenExternalLinks(false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->browser);

    connect(d->browser, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        Q_EMIT linkClicked(url.toString());
    });

    connect(d->browser, &QTextBrowser::highlighted, this, [this](const QUrl &url) {
        if (!url.isEmpty())
            Q_EMIT linkHovered(url.toString());
    });
}

ReadingView::~ReadingView() = default;

void ReadingView::setDocument(const Document &doc)
{
    // Note: Document is not copy-constructible (private constructor, no copy ctor),
    // so d->document cannot be updated here. Callers who need document() to reflect
    // the current state should use setMarkdown() instead.
    auto textDoc = d->renderer.renderToTextDocument(doc);
    d->browser->setHtml(textDoc->toHtml());
}

void ReadingView::setMarkdown(const QString &markdown)
{
    d->document = Document::fromMarkdown(markdown);
    setDocument(*d->document);
}

const Document *ReadingView::document() const
{
    return d->document.get();
}

void ReadingView::setTheme(const Theme &theme)
{
    d->theme = theme;
    if (theme.textFont != QFont())
        d->browser->setFont(theme.textFont);
}

Theme ReadingView::theme() const { return d->theme; }

void ReadingView::setRenderSettings(const RenderSettings &settings)
{
    d->renderSettings = settings;
    d->renderer.setSettings(settings);
}

RenderSettings ReadingView::renderSettings() const { return d->renderSettings; }

void ReadingView::setResourceProvider(ResourceProvider *provider)
{
    d->resourceProvider = provider;
}

qreal ReadingView::scrollFraction() const
{
    auto *sb = d->browser->verticalScrollBar();
    if (!sb || sb->maximum() == 0)
        return 0.0;
    return static_cast<qreal>(sb->value()) / sb->maximum();
}

void ReadingView::setScrollFraction(qreal fraction)
{
    auto *sb = d->browser->verticalScrollBar();
    if (sb && sb->maximum() > 0)
        sb->setValue(static_cast<int>(fraction * sb->maximum()));
}

void ReadingView::scrollToHeading(const HeadingInfo &heading)
{
    QTextDocument *doc = d->browser->document();
    QTextCursor cursor = doc->find(heading.text);
    if (!cursor.isNull()) {
        d->browser->setTextCursor(cursor);
        d->browser->ensureCursorVisible();
    }
}

int ReadingView::naturalHeight(int width) const
{
    QTextDocument *doc = d->browser->document();
    if (!doc) return 0;
    const qreal savedWidth = doc->textWidth();
    doc->setTextWidth(width);
    const int height = qRound(doc->size().height());
    doc->setTextWidth(savedWidth);
    return height;
}

} // namespace Markoff
