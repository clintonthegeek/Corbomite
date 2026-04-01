// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

class QTextDocument;
class QWidget;

namespace Corbomite {

class RenderedDocument {
public:
    ~RenderedDocument();

    QTextDocument *toQTextDocument() const;
    QWidget *createWidget(QWidget *parent = nullptr) const;

    static std::unique_ptr<RenderedDocument> fromQTextDocument(std::unique_ptr<QTextDocument> doc);

private:
    explicit RenderedDocument(std::unique_ptr<QTextDocument> doc);

    std::unique_ptr<QTextDocument> m_document;
};

} // namespace Corbomite
