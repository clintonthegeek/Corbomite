// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_READINGVIEW_H
#define MARKOFF_READINGVIEW_H
#include <QWidget>
#include <memory>
namespace Markoff {
class Document;
struct RenderSettings;
class ReadingView : public QWidget {
    Q_OBJECT
public:
    explicit ReadingView(QWidget *parent = nullptr);
    ~ReadingView() override;
    void setDocument(const Document &doc);
    void setSettings(const RenderSettings &settings);
Q_SIGNALS:
    void linkClicked(const QString &target);
private:
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace Markoff
#endif
