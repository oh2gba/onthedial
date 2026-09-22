// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/BandPlan.h"
#include <QtTest>

class TestBandPlan : public QObject
{
    Q_OBJECT
private slots:
    void builtInPlan()
    {
        const BandPlan plan = BandPlan::builtIn();
        QVERIFY(plan.size() > 50);
        QCOMPARE(plan.describe(6070, 1), QStringLiteral("49 m broadcast"));
        QCOMPARE(plan.describe(4625, 1), QString());                       // unallocated for us
        QCOMPARE(plan.describe(7125, 1), QStringLiteral("40 m amateur"));
        QCOMPARE(plan.describe(7250, 1), QStringLiteral("41 m broadcast"));
        QCOMPARE(plan.describe(7250, 2), QStringLiteral("40 m amateur"));   // Region 2 has 7000-7300
        QCOMPARE(plan.describe(7350, 2), QStringLiteral("41 m broadcast"));
        QCOMPARE(plan.describe(3600, 3), QStringLiteral("80 m amateur"));
        QCOMPARE(plan.describe(3950, 1), QStringLiteral("75 m broadcast \u00b7 75 m pirates (Europe)"));
        QCOMPARE(plan.describe(3950, 2), QStringLiteral("80 m amateur"));
        QCOMPARE(plan.describe(10000, 1), QStringLiteral("standard frequency and time (10 MHz)"));
        QCOMPARE(plan.describe(8992, 1), QStringLiteral("aeronautical mobile"));
        // informal uses come after the official allocation
        QCOMPARE(plan.describe(6300, 1), QStringLiteral("maritime mobile \u00b7 48 m pirate band (Europe)"));
        QCOMPARE(plan.describe(6300, 2), QStringLiteral("maritime mobile"));
        QCOMPARE(plan.describe(27500, 2), QStringLiteral("11 m freeband (unlicensed CB DX)"));
        QCOMPARE(plan.describe(13560, 1), QStringLiteral("ISM band (13.56 MHz, HiFER beacons)"));
        QCOMPARE(plan.describe(12577, 1), QStringLiteral("maritime mobile"));
        QCOMPARE(plan.describe(27185, 1), QStringLiteral("CB radio (11 m)"));

        QCOMPARE(plan.describe(198, 2), QStringLiteral("radio beacons (NDB)"));
        QCOMPARE(plan.describe(198, 1), QStringLiteral("long wave broadcast · radio beacons (NDB)"));
        QCOMPARE(plan.describe(350, 1), QStringLiteral("radio beacons (NDB)"));
        QCOMPARE(plan.describe(1000, 2), QStringLiteral("medium wave broadcast"));
        QCOMPARE(plan.lookup(14200, 1).first().kind, QStringLiteral("amateur"));
    }

    void rejectsGarbage()
    {
        BandPlan plan;
        QString err;
        QVERIFY(!plan.loadJson("not json", &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!plan.loadJson("{\"bands\": []}", &err));
    }
};

QTEST_GUILESS_MAIN(TestBandPlan)
#include "test_bandplan.moc"
