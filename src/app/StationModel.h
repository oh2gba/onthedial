// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/Schedule.h"
#include "core/Station.h"

#include <QAbstractTableModel>
#include <QDateTime>

class StationDb;

// Table of schedule entries around the tuned frequency with their live
// on-air status. Rows are pre-sorted: on air first, then by distance.
class StationModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column
    {
        ColDelta,
        ColFrequency,
        ColStatus,
        ColMode,
        ColStation,
        ColLanguage,
        ColTime,
        ColDays,
        ColCountry,
        ColSite,
        ColTarget,
        ColLastHeard,
        ColSource,
        ColRemarks,
        ColumnCount
    };
    enum Roles
    {
        SortRole = Qt::UserRole + 1,
        StatusRole,
        OnAirRankRole
    };

    explicit StationModel(StationDb* db, QObject* parent = nullptr);

    void setEntries(const StationList& entries, double centreKHz);
    void setCentre(double centreKHz);
    void refreshStatus(const QDateTime& utc = QDateTime::currentDateTimeUtc());
    int onAirCount() const;
    const StationEntry& entryAt(int row) const { return m_rows[row].entry; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

private:
    struct Row
    {
        StationEntry entry;
        Schedule::OnAir status = Schedule::OnAir::Unknown;
        double delta = 0.0;
    };
    static int rank(Schedule::OnAir s);
    static QString sourceLabel(const QString& id);
    QString languageOf(const StationEntry& e) const;
    QString siteOf(const StationEntry& e) const;
    void sortRows();
    QString tooltip(const Row& r) const;

    StationDb* m_db;
    QVector<Row> m_rows;
    double m_centre = 0.0;
    QDateTime m_lastEval;
};
