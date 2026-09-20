// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScheduleSource.h"
#include "EibiParser.h"
#include "StationDb.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

ScheduleSource::ScheduleSource(const QString& id, const QString& displayName,
                               const QUrl& defaultBase, StationDb* db,
                               QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_db(db)
    , m_nam(nam)
    , m_id(id)
    , m_name(displayName)
    , m_defaultBase(defaultBase)
{
    setBaseUrl(defaultBase);
}

void ScheduleSource::setBaseUrl(const QUrl& base)
{
    m_base = base.isValid() && !base.isEmpty() ? base : m_defaultBase;
    if (!m_base.path().endsWith(QLatin1Char('/')))
        m_base.setPath(m_base.path() + QLatin1Char('/'));
}

QString ScheduleSource::season() const
{
    return m_db->meta(m_id + QStringLiteral(".season"));
}

QDateTime ScheduleSource::lastUpdate() const
{
    return QDateTime::fromString(m_db->meta(m_id + QStringLiteral(".updated")), Qt::ISODate);
}

int ScheduleSource::count() const
{
    return m_db->count(m_id);
}

bool ScheduleSource::isStale(int maxAgeDays) const
{
    const QDateTime last = lastUpdate();
    if (!last.isValid() || count() == 0)
        return true;
    if (season() != EibiParser::seasonCode(QDate::currentDate()))
        return true;
    return last.daysTo(QDateTime::currentDateTimeUtc()) >= maxAgeDays;
}

QNetworkReply* ScheduleSource::get(const QUrl& url, const QString& ifModifiedSince)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("otd/") + QLatin1String(OTD_VERSION)
                      + QStringLiteral(" (+https://github.com/oh2gba/onthedial)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(90000);
    if (!ifModifiedSince.isEmpty())
        req.setRawHeader("If-Modified-Since", ifModifiedSince.toLatin1());
    return m_nam->get(req);
}

QNetworkReply* ScheduleSource::getFile(const QString& relativeName, const QString& ifModifiedSince)
{
    return get(m_base.resolved(QUrl(relativeName)), ifModifiedSince);
}

QString ScheduleSource::storedLastModified(const QString& season) const
{
    if (m_db->meta(m_id + QStringLiteral(".season")) != season || count() == 0)
        return QString();
    return m_db->meta(m_id + QStringLiteral(".lastModified"));
}

bool ScheduleSource::store(const QString& season, const StationList& entries,
                           const QString& lastModified)
{
    if (!m_db->replaceSource(m_id, entries))
        return false;
    m_db->setMeta(m_id + QStringLiteral(".season"), season);
    m_db->setMeta(m_id + QStringLiteral(".lastModified"), lastModified);
    touchUpdated();
    return true;
}

void ScheduleSource::touchUpdated()
{
    m_db->setMeta(m_id + QStringLiteral(".updated"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
}

bool ScheduleSource::begin()
{
    if (m_busy)
        return false;
    m_busy = true;
    return true;
}

void ScheduleSource::finish(bool ok, const QString& message)
{
    m_busy = false;
    emit finished(ok, message);
}
