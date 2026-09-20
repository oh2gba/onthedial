// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

// Which allocation a frequency falls into: broadcast metre bands, amateur
// bands, aeronautical and maritime mobile bands, standard time frequencies.
// Data comes from data/bandplan.json (compiled into the binary).
class BandPlan
{
public:
    struct Band
    {
        double fromKHz = 0.0;
        double toKHz = 0.0;
        QString kind;        // "broadcast", "amateur", "aero", "maritime", "time", "cb"
        QString name;        // "49 m broadcast"
        QVector<int> regions;  // empty = worldwide
        bool appliesTo(int region) const { return regions.isEmpty() || regions.contains(region); }
    };

    BandPlan();
    bool loadJson(const QByteArray& json, QString* error = nullptr);
    static BandPlan builtIn();

    int size() const { return m_bands.size(); }
    // Bands containing kHz for the given ITU region (1, 2 or 3).
    QVector<Band> lookup(double kHz, int region) const;
    // "49 m broadcast" or "40 m amateur · 41 m broadcast"; empty when unallocated
    QString describe(double kHz, int region) const;

private:
    QVector<Band> m_bands;
};
