// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Schedule.h"
#include <QtTest>

using Schedule::OnAir;

namespace QTest
{
template <>
inline char* toString(const OnAir& s)
{
    return qstrdup(Schedule::statusText(s).toUtf8().constData());
}
} // namespace QTest

class TestSchedule : public QObject
{
    Q_OBJECT

    static StationEntry entry(int start, int end, const QString& days = QString())
    {
        StationEntry e;
        e.kHz = 6000;
        e.startMin = start;
        e.endMin = end;
        e.days = days;
        e.persistence = 1;
        return e;
    }
    static QDateTime utc(int y, int m, int d, int hh, int mm)
    {
        return QDateTime(QDate(y, m, d), QTime(hh, mm), QTimeZone::utc());
    }

private slots:
    void dayNames()
    {
        const QDate mon(2026, 9, 21), tue(2026, 9, 22), fri(2026, 9, 25), sat(2026, 9, 26), sun(2026, 9, 27);
        QCOMPARE(Schedule::dayStatus("", mon), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("Mo-Fr", mon), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("Mo-Fr", sat), OnAir::No);
        QCOMPARE(Schedule::dayStatus("SaSu", sat), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("SaSu", fri), OnAir::No);
        QCOMPARE(Schedule::dayStatus("Tu,Fr", tue), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("Tu,Fr", fri), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("Tu,Fr", mon), OnAir::No);
        QCOMPARE(Schedule::dayStatus("We-Mo", mon), OnAir::Yes);   // wraps over the weekend
        QCOMPARE(Schedule::dayStatus("We-Mo", tue), OnAir::No);
        QCOMPARE(Schedule::dayStatus("156", mon), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("156", tue), OnAir::No);
        QCOMPARE(Schedule::dayStatus("7", sun), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("USB", sun), OnAir::Yes);
    }

    void specialTokens()
    {
        const QDate sat1(2026, 9, 5), sat2(2026, 9, 12), sat4(2026, 9, 26), fri(2026, 9, 25);
        QCOMPARE(Schedule::dayStatus("irr", fri), OnAir::Unknown);
        QCOMPARE(Schedule::dayStatus("Test", fri), OnAir::Unknown);
        QCOMPARE(Schedule::dayStatus("alt", fri), OnAir::Unknown);
        QCOMPARE(Schedule::dayStatus("altFr", fri), OnAir::Unknown);
        QCOMPARE(Schedule::dayStatus("altFr", sat1), OnAir::No);
        QCOMPARE(Schedule::dayStatus("1.Sa", sat1), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("1.Sa", sat2), OnAir::No);
        QCOMPARE(Schedule::dayStatus("Last6", sat4), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("Last6", sat2), OnAir::No);
        QCOMPARE(Schedule::dayStatus("15Sep", QDate(2026, 9, 15)), OnAir::Yes);
        QCOMPARE(Schedule::dayStatus("15Sep", QDate(2026, 9, 16)), OnAir::No);
        QCOMPARE(Schedule::dayStatus("MF-15", QDate(2026, 9, 14)), OnAir::Yes); // Monday 14th
        QCOMPARE(Schedule::dayStatus("MF-15", QDate(2026, 9, 21)), OnAir::No);
        QCOMPARE(Schedule::dayStatus("1WeFr", QDate(2026, 9, 2)), OnAir::Yes);   // 1st Wednesday
        QCOMPARE(Schedule::dayStatus("1WeFr", QDate(2026, 9, 4)), OnAir::Unknown); // a Friday
        QCOMPARE(Schedule::dayStatus("something", fri), OnAir::Unknown);
    }

    void timeWindows()
    {
        QCOMPARE(Schedule::status(entry(0, 1440), utc(2026, 9, 20, 12, 0)), OnAir::Yes);
        QCOMPARE(Schedule::status(entry(300, 360), utc(2026, 9, 20, 5, 0)), OnAir::Yes);
        QCOMPARE(Schedule::status(entry(300, 360), utc(2026, 9, 20, 5, 59)), OnAir::Yes);
        QCOMPARE(Schedule::status(entry(300, 360), utc(2026, 9, 20, 6, 0)), OnAir::No);
        QCOMPARE(Schedule::status(entry(300, 360), utc(2026, 9, 20, 4, 59)), OnAir::No);
        // wraps midnight
        QCOMPARE(Schedule::status(entry(1320, 300), utc(2026, 9, 20, 23, 30)), OnAir::Yes);
        QCOMPARE(Schedule::status(entry(1320, 300), utc(2026, 9, 20, 1, 0)), OnAir::Yes);
        QCOMPARE(Schedule::status(entry(1320, 300), utc(2026, 9, 20, 12, 0)), OnAir::No);
    }

    void midnightWrapUsesStartDay()
    {
        // Saturday-only broadcast 2300-0100: at Sunday 00:30 it is still on.
        StationEntry e = entry(1380, 60, "Sa");
        QCOMPARE(Schedule::status(e, utc(2026, 9, 27, 0, 30)), OnAir::Yes);  // Sunday 00:30
        QCOMPARE(Schedule::status(e, utc(2026, 9, 27, 23, 30)), OnAir::No);  // Sunday 23:30
        QCOMPARE(Schedule::status(e, utc(2026, 9, 26, 23, 30)), OnAir::Yes); // Saturday 23:30
    }

    void validityDatesAndSeasonFlags()
    {
        StationEntry e = entry(0, 1440);
        e.persistence = 6;
        e.startDate = "0102";   // 1 Feb
        e.stopDate = "2802";    // 28 Feb
        QCOMPARE(Schedule::status(e, utc(2026, 2, 10, 12, 0)), OnAir::Yes);
        QCOMPARE(Schedule::status(e, utc(2026, 3, 10, 12, 0)), OnAir::No);

        e.startDate = "0112";   // 1 Dec .. 31 Jan wraps the year
        e.stopDate = "3101";
        QCOMPARE(Schedule::status(e, utc(2026, 12, 25, 12, 0)), OnAir::Yes);
        QCOMPARE(Schedule::status(e, utc(2026, 1, 15, 12, 0)), OnAir::Yes);
        QCOMPARE(Schedule::status(e, utc(2026, 6, 15, 12, 0)), OnAir::No);

        StationEntry w = entry(0, 1440);
        w.persistence = 4;      // winter only
        QCOMPARE(Schedule::status(w, utc(2026, 7, 1, 12, 0)), OnAir::No);
        QCOMPARE(Schedule::status(w, utc(2026, 12, 1, 12, 0)), OnAir::Yes);
        w.persistence = 5;      // summer only
        QCOMPARE(Schedule::status(w, utc(2026, 7, 1, 12, 0)), OnAir::Yes);
        w.persistence = 8;      // inactive
        QCOMPARE(Schedule::status(w, utc(2026, 7, 1, 12, 0)), OnAir::Inactive);
        w.persistence = 91;     // utility, everlasting
        QCOMPARE(Schedule::status(w, utc(2026, 7, 1, 12, 0)), OnAir::Yes);
    }

    void formatting()
    {
        QCOMPARE(Schedule::timeWindow(entry(0, 1440)), QStringLiteral("24h"));
        QCOMPARE(Schedule::timeWindow(entry(65, 1380)), QStringLiteral("0105-2300"));
        QCOMPARE(Schedule::statusText(OnAir::Yes), QStringLiteral("on air"));
    }
};

QTEST_GUILESS_MAIN(TestSchedule)
#include "test_schedule.moc"
