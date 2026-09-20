// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

// One schedule line: a station transmitting on a frequency during a time
// window. Field semantics follow the EiBi CSV, other sources map onto it.
struct StationEntry
{
    qint64  id = 0;        // database row id, 0 when not stored
    QString source;        // "eibi", "hfcc", "aoki", or "user" for the personal list
    double  kHz = 0.0;
    int     startMin = 0;  // UTC, minutes since midnight
    int     endMin = 1440; // 1440 == 24:00
    QString days;          // "", "Mo-Fr", "1245", "irr", "1.Sa", "15Sep" ...
    QString itu;           // home country ITU code
    QString station;
    QString lang;          // language code
    QString target;        // target area code (or ITU code)
    QString site;          // transmitter site code, "/RUS-s" when relayed abroad
    int     persistence = 0;
    QString startDate;     // DDMM, valid-from (persistence 6)
    QString stopDate;      // DDMM, valid-until
    QString lastHeard;     // MMYY from "[0826]"
    QString remarks;       // free text: power, azimuth, notes
    QString langText;      // resolved language name (HFCC/Aoki); empty for EiBi
    QString siteText;      // resolved transmitter site (HFCC/Aoki); empty for EiBi
    QString mode;          // "AM", "USB", "LSB", "CW", "DRM", "RTTY", "FAX", "HFDL", "" unknown
};

using StationList = QVector<StationEntry>;

// Source id of the personal list kept in the database.
inline QString userSourceId() { return QStringLiteral("user"); }
