// SPDX-License-Identifier: GPL-3.0-or-later
#include "EibiSource.h"
#include "EibiParser.h"
#include "StationDb.h"

#include <QNetworkReply>

EibiSource::EibiSource(StationDb* db, QNetworkAccessManager* nam, QObject* parent)
    : ScheduleSource(QStringLiteral("eibi"), QStringLiteral("EiBi"),
                     QUrl(QStringLiteral("http://www.eibispace.de/dx/")), db, nam, parent)
{
}

void EibiSource::update()
{
    if (!begin())
        return;
    fetchSchedule(EibiParser::seasonCode(QDate::currentDate()), true);
}

void EibiSource::fetchSchedule(const QString& season, bool allowFallback)
{
    const QString file = QStringLiteral("sked-%1.csv").arg(season);
    emit progress(tr("Checking EiBi %1 ...").arg(file));
    QNetworkReply* reply = getFile(file, storedLastModified(season));
    connect(reply, &QNetworkReply::finished, this, [this, reply, season, allowFallback]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 304)
        {
            touchUpdated();
            finish(true, tr("EiBi %1 is up to date (%2 entries)").arg(season.toUpper()).arg(count()));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200)
        {
            // Around a season change the new file may not exist yet.
            if (allowFallback && (status == 404 || status == 403))
            {
                fetchSchedule(EibiParser::previousSeason(season), false);
                return;
            }
            finish(false, tr("EiBi download failed: %1")
                              .arg(status ? QString::number(status) : reply->errorString()));
            return;
        }

        const EibiParser::ParseResult parsed = EibiParser::parseCsv(reply->readAll());
        if (!parsed.error.isEmpty() || parsed.entries.size() < 100)
        {
            finish(false, tr("EiBi file unusable: %1").arg(parsed.error));
            return;
        }
        emit progress(tr("Storing %1 EiBi entries ...").arg(parsed.entries.size()));
        if (!store(season, parsed.entries, QString::fromLatin1(reply->rawHeader("Last-Modified"))))
        {
            finish(false, tr("Database error: %1").arg(m_db->lastError()));
            return;
        }
        fetchReadme(season, parsed.entries.size());
    });
}

void EibiSource::fetchReadme(const QString& season, int entryCount)
{
    emit progress(tr("Downloading EiBi README (code tables) ..."));
    QNetworkReply* reply = getFile(QStringLiteral("README.TXT"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, season, entryCount]() {
        reply->deleteLater();
        QString note;
        if (reply->error() == QNetworkReply::NoError)
        {
            const EibiParser::CodeTables tables = EibiParser::parseReadme(reply->readAll());
            if (!tables.isEmpty())
                m_db->storeCodes(tables);
            else
                note = tr(" (code tables not understood)");
        }
        else
            note = tr(" (README not available: %1)").arg(reply->errorString());

        finish(true, tr("EiBi %1 loaded, %2 entries%3").arg(season.toUpper()).arg(entryCount).arg(note));
    });
}
