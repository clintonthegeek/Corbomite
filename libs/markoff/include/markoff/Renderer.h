// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_RENDERER_H
#define MARKOFF_RENDERER_H

#include <memory>

class QTextDocument;

namespace Markoff {

class Document;
struct RenderSettings;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void setSettings(const RenderSettings &settings);
    RenderSettings settings() const;

    std::unique_ptr<QTextDocument> renderToTextDocument(const Document &doc) const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Markoff

#endif // MARKOFF_RENDERER_H
