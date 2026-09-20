// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Station.h"

#include <QDateTime>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class StationDb;

// Base class of a downloadable schedule database. Each source keeps its
// bookkeeping (season, last update, Last-Modified) in the StationDb meta
// table under "<id>.*" keys.
class ScheduleSource : public QObject
{
    Q_OBJECT
public:
    ScheduleSource(const QString& id, const QString& displayName, const QUrl& defaultBase,
                   StationDb* db, QNetworkAccessManager* nam, QObject* parent = nullptr);

    QString id() const { return m_id; }
    QString displayName() const { return m_name; }

    void setBaseUrl(const QUrl& base);
    QUrl baseUrl() const { return m_base; }
    QUrl defaultBaseUrl() const { return m_defaultBase; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on) { m_enabled = on; }
    bool isBusy() const { return m_busy; }

    QString season() const;
    QDateTime lastUpdate() const;
    bool isStale(int maxAgeDays) const;
    int count() const;

public slots:
    // Starts the (asynchronous) refresh; emits finished() exactly once.
    virtual void update() = 0;

signals:
    void progress(const QString& message);
    void finished(bool ok, const QString& message);

protected:
    QNetworkReply* get(const QUrl& url, const QString& ifModifiedSince = QString());
    QNetworkReply* getFile(const QString& relativeName, const QString& ifModifiedSince = QString());
    // Last-Modified we hold for the given season file, or empty when the
    // stored data is not for this season.
    QString storedLastModified(const QString& season) const;
    bool store(const QString& season, const StationList& entries, const QString& lastModified);
    void touchUpdated();
    void finish(bool ok, const QString& message);
    bool begin();

    StationDb* m_db;
    QNetworkAccessManager* m_nam;

private:
    QString m_id;
    QString m_name;
    QUrl m_base;
    QUrl m_defaultBase;
    bool m_enabled = true;
    bool m_busy = false;
};
