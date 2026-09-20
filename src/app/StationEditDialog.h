// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Station.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTimeEdit;
class QPlainTextEdit;

// Add or edit one entry of the personal station list.
class StationEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StationEditDialog(const StationEntry& entry, QWidget* parent = nullptr);
    StationEntry entry() const;

private:
    StationEntry m_entry;
    QLineEdit* m_station;
    QDoubleSpinBox* m_kHz;
    QComboBox* m_mode;
    QCheckBox* m_allDay;
    QTimeEdit* m_start;
    QTimeEdit* m_end;
    QLineEdit* m_days;
    QLineEdit* m_country;
    QLineEdit* m_language;
    QLineEdit* m_site;
    QPlainTextEdit* m_notes;
};
