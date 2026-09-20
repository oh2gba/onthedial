// SPDX-License-Identifier: GPL-3.0-or-later
#include "HfccSource.h"
#include "EibiParser.h"
#include "HfccParser.h"
#include "StationDb.h"
#include "ZipReader.h"

#include <QNetworkReply>

HfccSource::HfccSource(StationDb* db, QNetworkAccessManager* nam, QObject* parent)
    : ScheduleSource(QStringLiteral("hfcc"), QStringLiteral("HFCC"),
                     QUrl(QStringLiteral("http://www.hfcc.org/data/")), db, nam, parent)
{
}

void HfccSource::update()
{
    if (!begin())
        return;
    fetchZip(EibiParser::seasonCode(QDate::currentDate()), true);
}

void HfccSource::fetchZip(const QString& season, bool allowFallback)
{
    const QString file = QStringLiteral("%1/%1allx2.zip").arg(season);
    emit progress(tr("Checking HFCC %1 ...").arg(file));
    QNetworkReply* reply = getFile(file, storedLastModified(season));
    connect(reply, &QNetworkReply::finished, this, [this, reply, season, allowFallback]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 304)
        {
            touchUpdated();
            finish(true, tr("HFCC %1 is up to date (%2 entries)").arg(season.toUpper()).arg(count()));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200)
        {
            if (allowFallback && (status == 404 || status == 403))
            {
                fetchZip(EibiParser::previousSeason(season), false);
                return;
            }
            finish(false, tr("HFCC download failed: %1")
                              .arg(status ? QString::number(status) : reply->errorString()));
            return;
        }

        QString zipError;
        const QHash<QString, QByteArray> files = ZipReader::extractAll(reply->readAll(), &zipError);
        const QString skedName = ZipReader::findName(files, QStringLiteral("^[ab]\\d\\dall\\d\\d\\.txt$"));
        if (skedName.isEmpty())
        {
            finish(false, tr("HFCC archive has no schedule file (%1)").arg(zipError));
            return;
        }
        auto fileNamed = [&files](const QString& pattern) {
            const QString name = ZipReader::findName(files, pattern);
            return name.isEmpty() ? QByteArray() : files.value(name);
        };
        const HfccParser::Tables tables = HfccParser::parseTables(
            fileNamed(QStringLiteral("^site\\.txt$")), fileNamed(QStringLiteral("^broadcas\\.txt$")),
            fileNamed(QStringLiteral("^language\\.txt$")), fileNamed(QStringLiteral("^admin\\.txt$")));

        const HfccParser::ParseResult parsed = HfccParser::parseSchedule(files.value(skedName), tables);
        if (!parsed.error.isEmpty() || parsed.entries.size() < 100)
        {
            finish(false, tr("HFCC schedule unusable: %1").arg(parsed.error));
            return;
        }
        emit progress(tr("Storing %1 HFCC entries ...").arg(parsed.entries.size()));
        if (!store(season, parsed.entries, QString::fromLatin1(reply->rawHeader("Last-Modified"))))
        {
            finish(false, tr("Database error: %1").arg(m_db->lastError()));
            return;
        }
        finish(true, tr("HFCC %1 loaded, %2 entries").arg(season.toUpper()).arg(parsed.entries.size()));
    });
}
