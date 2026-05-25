// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/models/PropertyType.h"

#include <markoff/parser/YamlValue.h>

#include <QtWidgets/QWidget>

class QCheckBox;
class QDateEdit;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLineEdit;
class QVBoxLayout;

namespace Corbomite {

/// Abstract base: one editor for one frontmatter key's value.
/// Caller sets initial value via `setValue`; editor emits `valueChanged`
/// on user edit. Caller reads new value via `currentValue`.
class PropertyEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit PropertyEditorWidget(QWidget *parent = nullptr);
    ~PropertyEditorWidget() override = default;

    virtual void setValue(const Markoff::YamlValue &value) = 0;
    virtual Markoff::YamlValue currentValue() const = 0;

Q_SIGNALS:
    void valueChanged();
};

// --- Concrete editors ---------------------------------------------------

class TextPropertyEditor : public PropertyEditorWidget {
    Q_OBJECT
public:
    explicit TextPropertyEditor(QWidget *parent = nullptr);
    void setValue(const Markoff::YamlValue &value) override;
    Markoff::YamlValue currentValue() const override;
private:
    QLineEdit *m_edit;
};

class NumberPropertyEditor : public PropertyEditorWidget {
    Q_OBJECT
public:
    explicit NumberPropertyEditor(QWidget *parent = nullptr);
    void setValue(const Markoff::YamlValue &value) override;
    Markoff::YamlValue currentValue() const override;
private:
    QDoubleSpinBox *m_spin;
    bool m_isInt = false;  // tracks whether original value was integer-typed
};

class CheckboxPropertyEditor : public PropertyEditorWidget {
    Q_OBJECT
public:
    explicit CheckboxPropertyEditor(QWidget *parent = nullptr);
    void setValue(const Markoff::YamlValue &value) override;
    Markoff::YamlValue currentValue() const override;
private:
    QCheckBox *m_check;
};

class DatePropertyEditor : public PropertyEditorWidget {
    Q_OBJECT
public:
    explicit DatePropertyEditor(QWidget *parent = nullptr);
    void setValue(const Markoff::YamlValue &value) override;
    Markoff::YamlValue currentValue() const override;
private:
    QDateEdit *m_date;
};

class DateTimePropertyEditor : public PropertyEditorWidget {
    Q_OBJECT
public:
    explicit DateTimePropertyEditor(QWidget *parent = nullptr);
    void setValue(const Markoff::YamlValue &value) override;
    Markoff::YamlValue currentValue() const override;
private:
    QDateTimeEdit *m_dt;
};

class ListPropertyEditor : public PropertyEditorWidget {
    Q_OBJECT
public:
    explicit ListPropertyEditor(QWidget *parent = nullptr);
    void setValue(const Markoff::YamlValue &value) override;
    Markoff::YamlValue currentValue() const override;

private Q_SLOTS:
    void addRow(const QString &initial = QString());
    void removeRow(QWidget *row);

private:
    QVBoxLayout *m_rowsLayout;
    QVector<QLineEdit *> m_edits;
    QVector<QWidget *> m_rows;
    QWidget *m_rowsContainer;
};

/// Factory: given a type + initial value, return the right editor widget.
PropertyEditorWidget *makePropertyEditor(PropertyType type,
                                         const Markoff::YamlValue &initialValue,
                                         QWidget *parent = nullptr);

}  // namespace Corbomite
