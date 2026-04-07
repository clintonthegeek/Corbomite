// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_RENDERER_H
#define MARKOFF_RENDERER_H

#include <memory>

class QTextDocument;

namespace Markoff {

class Document;
struct RenderSettings;
struct Theme;
class ResourceProvider;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void setSettings(const RenderSettings &settings);
    RenderSettings settings() const;

    /// Set a non-owning resource provider used to resolve relative image paths.
    /// May be nullptr (in which case image src values are used as-is).
    void setResourceProvider(ResourceProvider *provider);

    /// Provide a theme so reading-view CSS can pick up colors (blockquote
    /// border, code block background, footnote text) instead of using
    /// hardcoded fallbacks.
    void setTheme(const Theme &theme);

    std::unique_ptr<QTextDocument> renderToTextDocument(const Document &doc) const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Markoff

#endif // MARKOFF_RENDERER_H
