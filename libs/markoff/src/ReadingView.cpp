// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/ReadingView.h"
#include "markoff/Document.h"
#include "markoff/Renderer.h"
#include "markoff/RenderSettings.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QUrl>

namespace Markoff {

struct ReadingView::Private {
    QTextBrowser *browser = nullptr;
    Markoff::Renderer renderer;
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
}

ReadingView::~ReadingView() = default;

void ReadingView::setDocument(const Document &doc)
{
    auto textDoc = d->renderer.renderToTextDocument(doc);
    d->browser->setHtml(textDoc->toHtml());
}

void ReadingView::setSettings(const RenderSettings &settings)
{
    d->renderer.setSettings(settings);
}

} // namespace Markoff
