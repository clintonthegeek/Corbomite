// SPDX-License-Identifier: GPL-3.0-or-later
// TODO(Task 7): Stub — old ReadingView was deleted in Task 4.
// NoteEditorWidget still references this class; Task 7 will remove
// the ReadingView usage and delete this stub.
#ifndef MARKOFF_READINGVIEW_H
#define MARKOFF_READINGVIEW_H

#include <QWidget>
#include <QString>

namespace Markoff {

class ResourceProvider;

class ReadingView : public QWidget {
    Q_OBJECT
public:
    explicit ReadingView(QWidget *parent = nullptr) : QWidget(parent) {}

    void setMarkdown(const QString &) {}
    void setResourceProvider(ResourceProvider *) {}

Q_SIGNALS:
    void linkClicked(const QString &target);
};

} // namespace Markoff

#endif // MARKOFF_READINGVIEW_H
