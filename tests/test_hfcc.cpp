// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/HfccParser.h"
#include <QtTest>

class TestHfcc : public QObject
{
    Q_OBJECT

    static HfccParser::Tables tables()
    {
        const QByteArray site =
            ";         20-May-2026 SITE.TXT REFERENCE TABLE\r\n"
            ";--+------------------------------+---+-----+------\r\n"
            ";Co Site Name                      ADM Lati  Longi\r\n"
            "A-A Alma Ata                       KAZ 43N17 077E00\r\n"
            "BEC Bechar                         ALG 31N34 002W21\r\n"
            "GAL Galbeni                        ROU 46N44 026E50\r\n"
            "MOS Moosbrunn                      AUT 48N00 016E28\r\n";
        const QByteArray brc =
            ";CO BROADCASTER\r\n"
            "RRO Radio Romania International\r\n"
            "TDA Telediffusion d'Algerie\r\n";
        const QByteArray lang =
            "\xEF\xBB\xBF;         23-JAN-2014  Reference Table Language\r\n"
            "Arb Standard Arabic                     \r\n"
            "Ron Romanian                            \r\n";
        const QByteArray adm =
            ";CO ADMINISTRATION ENGLISH NAME                        ADMINISTRATION FRENCH NAME\r\n"
            "ALG Algeria                                            Alg\xE9rie                                           Argelia\r\n"
            "AUT Austria                                            Autriche                                          Austria\r\n"
            "ROU Romania                                            Roumanie                                          Rumania\r\n";
        return HfccParser::parseTables(site, brc, lang, adm);
    }

private slots:
    void referenceTables()
    {
        const auto t = tables();
        QCOMPARE(t.sites.size(), 4);
        QCOMPARE(t.sites.value("A-A").name, QStringLiteral("Alma Ata"));
        QCOMPARE(t.sites.value("GAL").adm, QStringLiteral("ROU"));
        QCOMPARE(t.broadcasters.value("RRO"), QStringLiteral("Radio Romania International"));
        QCOMPARE(t.languages.value("Arb"), QStringLiteral("Standard Arabic"));
        QCOMPARE(t.admins.value("ALG"), QStringLiteral("Algeria"));
        QCOMPARE(t.admins.value("ROU"), QStringLiteral("Romania"));
    }

    void schedule()
    {
        const QByteArray data =
            "; A26 ALL 18-sep-2026\r\n"
            "; Global HF Schedule\r\n"
            ";----+----+----+------------------------------+---+----+-------+---+---+-------+------+------+-+-----+----------+---+---+---+-----+-+-----+-----+-----+-------\r\n"
            ";FREQ STRT STOP CIRAF ZONES                    LOC POWR AZIMUTH SLW ANT DAYS    FDATE  TDATE MOD AFRQ LANGUAGE   ADM BRC FMO REQ# OLD ALT1 ALT2  ALT3  NOTES\r\n"
            ";----+----+----+------------------------------+---+----+-------+---+---+-------+------+------+-+-----+----------+---+---+---+-----+-+-----+-----+-----+-------\r\n"
            " 9500 0300 0400 37SE,38SW,46E,47NW             BEC  300 131       0 146 1234567 290326 251026 D  7778 Arb        ALG TDA TDA  2498                     \r\n"
            " 9500 0400 0500 27SE                           GAL  300 285       0 206 1234567 290326 251026 D       Ron        ROU RRO ROU   985                     \r\n"
            " 6155 1500 1600 28                             MOS  100 0         0 900 23456   010626 310826 N       Ron        ROU RRO ROU  2347                     NOTE\r\n"
            " 3210 0000 2400 18,27,28,37                    NIJ   15 0         0 750 1234567 300326 251026 D       Nld        HOL OMR OMR  4853                     \r\n"
            "garbage line\r\n";
        const auto r = HfccParser::parseSchedule(data, tables());
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.entries.size(), 4);
        QCOMPARE(r.skippedLines, 1);

        const StationEntry& a = r.entries[0];
        QCOMPARE(a.source, QStringLiteral("hfcc"));
        QCOMPARE(a.kHz, 9500.0);
        QCOMPARE(a.startMin, 180);
        QCOMPARE(a.endMin, 240);
        QVERIFY(a.days.isEmpty());
        QCOMPARE(a.itu, QStringLiteral("ALG"));
        QCOMPARE(a.station, QStringLiteral("Telediffusion d'Algerie"));
        QCOMPARE(a.lang, QStringLiteral("Arb"));
        QCOMPARE(a.langText, QStringLiteral("Standard Arabic"));
        QCOMPARE(a.site, QStringLiteral("BEC"));
        QCOMPARE(a.siteText, QStringLiteral("Bechar"));
        QCOMPARE(a.target, QStringLiteral("37SE,38SW,46E,47NW"));
        QCOMPARE(a.startDate, QStringLiteral("2903"));
        QCOMPARE(a.stopDate, QStringLiteral("2510"));
        QCOMPARE(a.remarks, QStringLiteral("300 kW, az 131°"));
        QCOMPARE(a.mode, QStringLiteral("AM"));

        const StationEntry& b = r.entries[2];
        QCOMPARE(b.days, QStringLiteral("23456"));
        QCOMPARE(b.siteText, QStringLiteral("Moosbrunn (Austria)"));   // relay abroad
        QCOMPARE(b.startDate, QStringLiteral("0106"));
        QCOMPARE(b.stopDate, QStringLiteral("3108"));
        QCOMPARE(b.remarks, QStringLiteral("100 kW, NOTE"));
        QCOMPARE(b.mode, QStringLiteral("DRM"));

        const StationEntry& c = r.entries[3];
        QCOMPARE(c.startMin, 0);
        QCOMPARE(c.endMin, 1440);
        QCOMPARE(c.station, QStringLiteral("OMR"));   // unknown broadcaster keeps its code
        QVERIFY(c.siteText.isEmpty());                // unknown site
    }

    void missingRulerIsAnError()
    {
        const auto r = HfccParser::parseSchedule(" 9500 0300 0400 ...\r\n", HfccParser::Tables());
        QVERIFY(!r.error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestHfcc)
#include "test_hfcc.moc"
