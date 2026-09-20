// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QTableView;
class StationDb;
class StationModel;

// Manage the personal station list: add, edit, delete, import, export.
class MyStationsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MyStationsDialog(StationDb* db, double currentKHz, const QString& currentMode,
                              QWidget* parent = nullptr);

private slots:
    void reload();
    void addEntry();
    void editEntry();
    void removeEntry();
    void importCsv();
    void exportCsv();

private:
    int selectedRow() const;

    StationDb* m_db;
    double m_currentKHz;
    QString m_currentMode;
    StationModel* m_model;
    QTableView* m_table;
};
