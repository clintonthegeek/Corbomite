// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertyRow.h"

#include "PropertyEditorWidget.h"

#include <KLocalizedString>

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QToolButton>

namespace Corbomite {

bool isEditableFrontmatterValue(const Markoff::YamlValue &value)
{
    using Kind = Markoff::YamlValue::Kind;
    switch (value.kind()) {
    case Kind::Null:
    case Kind::Bool:
    case Kind::Int:
    case Kind::Double:
    case Kind::String:
        return true;
    case Kind::Seq:
        for (int i = 0, n = value.size(); i < n; ++i) {
            const auto el = value.at(i);
            if (el.kind() == Kind::Seq || el.kind() == Kind::Map) return false;
        }
        return true;
    case Kind::Map:
    default:
        return false;
    }
}

PropertyRow::PropertyRow(const QString &key, PropertyType type,
                         const Markoff::YamlValue &value, bool editable,
                         QWidget *parent)
    : QWidget(parent), m_key(key), m_type(type), m_editable(editable)
{
    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    m_grip = new QToolButton(this);
    m_grip->setIcon(QIcon::fromTheme(QStringLiteral("application-menu")));  // grip affordance; drag wired in Task 7
    m_grip->setAutoRaise(true);
    m_grip->setCursor(Qt::SizeAllCursor);
    m_grip->setToolTip(i18n("Drag to reorder"));
    h->addWidget(m_grip);

    m_keyStack = new QStackedWidget(this);
    m_keyLabel = new QLabel(key, this);
    m_keyEdit = new QLineEdit(key, this);
    m_keyStack->addWidget(m_keyLabel);
    m_keyStack->addWidget(m_keyEdit);
    m_keyStack->setCurrentWidget(m_keyLabel);
    h->addWidget(m_keyStack);

    if (editable) {
        m_editor = makePropertyEditor(type, value, this);
        connect(m_editor, &PropertyEditorWidget::valueChanged,
                this, &PropertyRow::valueChanged);
        h->addWidget(m_editor, 1);
    } else {
        auto *summary = new QLabel(
            value.kind() == Markoff::YamlValue::Kind::Map
                ? i18n("{...} (not editable here)")
                : i18n("[...] (not editable here)"),
            this);
        summary->setStyleSheet(QStringLiteral("color: gray;"));
        h->addWidget(summary, 1);
    }

    m_deleteButton = new QToolButton(this);
    m_deleteButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_deleteButton->setAutoRaise(true);
    m_deleteButton->setToolTip(i18n("Delete property"));
    connect(m_deleteButton, &QToolButton::clicked,
            this, &PropertyRow::deleteRequested);
    h->addWidget(m_deleteButton);

    if (editable) {
        m_keyLabel->setCursor(Qt::IBeamCursor);
        m_keyLabel->installEventFilter(this);
        connect(m_keyEdit, &QLineEdit::editingFinished,
                this, &PropertyRow::commitInlineRename);
    }
}

bool PropertyRow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_keyLabel && ev->type() == QEvent::MouseButtonRelease) {
        beginInlineRename();
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

Markoff::YamlValue PropertyRow::currentValue() const
{
    if (m_editor) return m_editor->currentValue();
    return Markoff::YamlValue();
}

void PropertyRow::beginInlineRename()
{
    m_keyEdit->setText(m_key);
    m_keyStack->setCurrentWidget(m_keyEdit);
    m_keyEdit->setFocus();
    m_keyEdit->selectAll();
}

void PropertyRow::commitInlineRename()
{
    const QString proposed = m_keyEdit->text().trimmed();
    m_keyStack->setCurrentWidget(m_keyLabel);
    if (proposed.isEmpty() || proposed == m_key) return;
    Q_EMIT keyRenameRequested(m_key, proposed);
}

}  // namespace Corbomite
