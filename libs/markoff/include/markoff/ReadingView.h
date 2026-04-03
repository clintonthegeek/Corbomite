// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_READINGVIEW_H
#define MARKOFF_READINGVIEW_H

#include <QWidget>
#include <memory>
#include <markoff/Theme.h>
#include <markoff/RenderSettings.h>
#include <markoff/Document.h>

namespace Markoff {

class ResourceProvider;

class ReadingView : public QWidget {
    Q_OBJECT
public:
    explicit ReadingView(QWidget *parent = nullptr);
    ~ReadingView() override;

    // Content
    void setDocument(const Document &doc);
    void setMarkdown(const QString &markdown);
    const Document *document() const;

    // Configuration
    void setTheme(const Theme &theme);
    Theme theme() const;

    void setRenderSettings(const RenderSettings &settings);
    RenderSettings renderSettings() const;

    void setResourceProvider(ResourceProvider *provider);

    // Scroll
    qreal scrollFraction() const;
    void setScrollFraction(qreal fraction);
    void scrollToHeading(const HeadingInfo &heading);

    // Size hint for embedding contexts
    int naturalHeight(int width) const;

Q_SIGNALS:
    void linkClicked(const QString &target);
    void linkHovered(const QString &target);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Markoff

#endif // MARKOFF_READINGVIEW_H
