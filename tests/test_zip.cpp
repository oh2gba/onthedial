// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/ZipReader.h"
#include <QtTest>

class TestZip : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip()
    {
        QHash<QString, QByteArray> in;
        in.insert("A26all00.TXT", QByteArray("hello\r\nworld\r\n").repeated(200));
        in.insert("site.txt", "ABC Site");
        const QByteArray zip = ZipReader::create(in);
        QVERIFY(zip.size() > 40);

        QString err;
        const auto out = ZipReader::extractAll(zip, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.value("site.txt"), QByteArray("ABC Site"));
        QCOMPARE(out.value("A26all00.TXT"), in.value("A26all00.TXT"));

        QCOMPARE(ZipReader::findName(out, "^[ab]\\d\\dall\\d\\d\\.txt$"), QStringLiteral("A26all00.TXT"));
        QCOMPARE(ZipReader::findName(out, "^xta26\\.txt$"), QString());
    }

    void rejectsGarbage()
    {
        QString err;
        const auto out = ZipReader::extractAll("this is not a zip file at all", &err);
        QVERIFY(out.isEmpty());
        QVERIFY(!err.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestZip)
#include "test_zip.moc"
