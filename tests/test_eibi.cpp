// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/EibiParser.h"
#include <QtTest>

class TestEibi : public QObject
{
    Q_OBJECT
private slots:
    void parsesTypicalLines()
    {
        const QByteArray csv =
            "kHz:75;Time(UTC):93;Days:59;ITU:49;Station:201;Lng:49;Target:62;Remarks:135;P:35;Start:60;Stop:60;\r\n"
            "4625;0000-2400;;RUS;The Buzzer;;RUS;B2;1;;[0826]\r\n"
            "4610;2200-0500;;G;GYA Northwood Meteo Fax;;WEu;nw;1;;\r\n"
            "9500;0500-0600;Mo-Fr;ROU;Radio Romania Int.;E;Eu;/AUT-m;6;0102;2802[0219]\r\n"
            "junk line without fields\r\n"
            "16.3;0000-2400;;IND;VTX1 Indian Navy;;SAs;v;1;;\r\n";
        const auto r = EibiParser::parseCsv(csv);
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.entries.size(), 4);
        QCOMPARE(r.skippedLines, 1);

        const StationEntry& buzzer = r.entries[0];
        QCOMPARE(buzzer.kHz, 4625.0);
        QCOMPARE(buzzer.startMin, 0);
        QCOMPARE(buzzer.endMin, 1440);
        QCOMPARE(buzzer.station, QStringLiteral("The Buzzer"));
        QCOMPARE(buzzer.itu, QStringLiteral("RUS"));
        QCOMPARE(buzzer.site, QStringLiteral("B2"));
        QCOMPARE(buzzer.persistence, 1);
        QCOMPARE(buzzer.lastHeard, QStringLiteral("0826"));
        QVERIFY(buzzer.stopDate.isEmpty());

        const StationEntry& fax = r.entries[1];
        QCOMPARE(fax.startMin, 22 * 60);
        QCOMPARE(fax.endMin, 5 * 60);

        const StationEntry& rri = r.entries[2];
        QCOMPARE(rri.days, QStringLiteral("Mo-Fr"));
        QCOMPARE(rri.lang, QStringLiteral("E"));
        QCOMPARE(rri.site, QStringLiteral("/AUT-m"));
        QCOMPARE(rri.persistence, 6);
        QCOMPARE(rri.startDate, QStringLiteral("0102"));
        QCOMPARE(rri.stopDate, QStringLiteral("2802"));
        QCOMPARE(rri.lastHeard, QStringLiteral("0219"));

        QCOMPARE(r.entries[3].kHz, 16.3);

        QCOMPARE(buzzer.mode, QStringLiteral("AM"));
        QCOMPARE(fax.mode, QStringLiteral("FAX"));
    }

    void modeGuessing()
    {
        StationEntry e;
        e.station = "Some Radio";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("AM"));
        e.days = "USB";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("USB"));
        e.days.clear();
        e.lang = "-CW";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("CW"));
        e.lang = "-TY";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("RTTY"));
        e.lang = "-HF";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("HFDL"));
        e.lang.clear();
        e.station = "Shannon Volmet";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("USB"));
        e.station = "Radio Kuwait DRM";
        QCOMPARE(EibiParser::guessMode(e), QStringLiteral("DRM"));
    }

    void latin1IsDecoded()
    {
        const QByteArray csv = "6000;0000-0100;;D;Radio B\xfcrgerfunk;D;Eu;;1;;\n";
        const auto r = EibiParser::parseCsv(csv);
        QCOMPARE(r.entries.size(), 1);
        QCOMPARE(r.entries[0].station, QString::fromUtf8("Radio Bürgerfunk"));
    }

    void emptyInputIsAnError()
    {
        const auto r = EibiParser::parseCsv("kHz:75;Time(UTC):93\n");
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.entries.isEmpty());
    }

    void seasonCodes()
    {
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 9, 20)), QStringLiteral("a26"));
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 2, 1)),  QStringLiteral("b25"));
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 3, 28)), QStringLiteral("b25")); // Saturday
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 3, 29)), QStringLiteral("a26")); // last Sunday
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 10, 24)), QStringLiteral("a26"));
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 10, 25)), QStringLiteral("b26"));
        QCOMPARE(EibiParser::seasonCode(QDate(2026, 12, 31)), QStringLiteral("b26"));
        QCOMPARE(EibiParser::seasonCode(QDate(2027, 1, 1)),  QStringLiteral("b26"));

        QCOMPARE(EibiParser::previousSeason(QStringLiteral("a26")), QStringLiteral("b25"));
        QCOMPARE(EibiParser::previousSeason(QStringLiteral("b26")), QStringLiteral("a26"));
        QCOMPARE(EibiParser::nextSeason(QStringLiteral("a26")), QStringLiteral("b26"));
        QCOMPARE(EibiParser::nextSeason(QStringLiteral("b26")), QStringLiteral("a27"));
        QCOMPARE(EibiParser::previousSeason(QStringLiteral("a00")), QStringLiteral("b99"));
    }

    void readmeTables()
    {
        const QByteArray readme =
            "D) Codes used.\n"
            "   I)   Language codes.\n"
            "   II)  Country codes.\n"
            "\n"
            "   I) Language codes.\n"
            "   \n"
            "   -CW   Morse Station\n"
            "   E     English: UK (60m), USA (225m), India (200m), others               [eng]\n"
            "   DI    Dinka: South Sudan (1.4m)                           [dip,diw,dik,dib,dks]\n"
            "         Fujian: see TW-Taiwanese\n"
            "\n"
            "   II) Country codes.\n"
            "   Asterisks (*) denote non-official abbreviations\n"
            "   RUS  Russia\n"
            "   G    United Kingdom\n"
            "   CAB  Cabinda *\n"
            "\n"
            "   III) Target-area codes.\n"
            "   Eu  - Europe (often including North Africa/Middle East)\n"
            "   C.. - Central ..\n"
            "\n"
            "   IV) Transmitter site codes.\n"
            "   One-letter or two-letter codes.\n"
            "   AFS: Meyerton 26S35-28E08 except:\n"
            "        ct-Cape Town 33S41-18E42\n"
            "   ARM: Gavar (formerly Kamo) 40N25-45E12\n"
            "        y-Yerevan 40N10-44E30\n"
            "   RUS: s-Samara 53N17-50E15\n"
            "        B2-Kerro, near St. Petersburg 60N18-30E17\n";
        const auto t = EibiParser::parseReadme(readme);
        QCOMPARE(t.languages.value("E"), QStringLiteral("English: UK (60m), USA (225m), India (200m), others"));
        QCOMPARE(t.languages.value("-CW"), QStringLiteral("Morse Station"));
        QCOMPARE(t.languages.value("DI"), QStringLiteral("Dinka: South Sudan (1.4m)"));
        QCOMPARE(t.countries.value("RUS"), QStringLiteral("Russia"));
        QCOMPARE(t.countries.value("G"), QStringLiteral("United Kingdom"));
        QCOMPARE(t.countries.value("CAB"), QStringLiteral("Cabinda"));
        QCOMPARE(t.targets.value("Eu"), QStringLiteral("Europe (often including North Africa/Middle East)"));
        QCOMPARE(t.sites.value("AFS/"), QStringLiteral("Meyerton"));
        QCOMPARE(t.sites.value("AFS/ct"), QStringLiteral("Cape Town"));
        QCOMPARE(t.sites.value("ARM/"), QStringLiteral("Gavar (formerly Kamo)"));
        QCOMPARE(t.sites.value("ARM/y"), QStringLiteral("Yerevan"));
        QCOMPARE(t.sites.value("RUS/s"), QStringLiteral("Samara"));
        QCOMPARE(t.sites.value("RUS/B2"), QStringLiteral("Kerro, near St. Petersburg"));
    }
};

QTEST_GUILESS_MAIN(TestEibi)
#include "test_eibi.moc"
