// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class StationDb;

// Asks the project web page once a day for the current version. The request
// carries only the program's own version and platform; the answer is a small
// JSON document {version, url, message}. Results are cached in the database
// so the label survives restarts without a new request.
class UpdateCheck : public QObject
{
    Q_OBJECT
public:
    struct Result
    {
        QString latest;    // version on the server
        QString url;       // where to get it
        QString message;   // optional short note from the project
        bool newer = false;
        bool valid = false;
    };

    UpdateCheck(StationDb* db, QNetworkAccessManager* nam, QObject* parent = nullptr);

    // "1.2.10" vs "1.2.9" -> 1; equal -> 0; handles missing parts and suffixes.
    static int compareVersions(const QString& a, const QString& b);
    // "linux", "linux-flatpak", "linux-appimage", "windows", "macos"
    static QString platformName();
    static Result parseResponse(const QByteArray& json, const QString& currentVersion);

    Result cached() const;
    QDateTime lastCheck() const;

public slots:
    // Starts a request unless one was made in the last 24 hours (or force).
    void run(const QUrl& endpoint, bool force = false);

signals:
    void finished(const UpdateCheck::Result& result);

private:
    StationDb* m_db;
    QNetworkAccessManager* m_nam;
    bool m_busy = false;
};
