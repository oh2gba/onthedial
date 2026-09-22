// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

// Asks the project web page for the current version. The request carries
// only the program's own version and platform; the answer is a small JSON
// document {version, url, message}. Nothing is remembered: a failed request
// simply yields no result, and the caller asks again another day.
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

    explicit UpdateCheck(QNetworkAccessManager* nam, QObject* parent = nullptr);

    // "1.2.10" vs "1.2.9" -> 1; equal -> 0; handles missing parts and suffixes.
    static int compareVersions(const QString& a, const QString& b);
    // "linux", "linux-flatpak", "linux-appimage", "windows", "macos"
    static QString platformName();
    static Result parseResponse(const QByteArray& json, const QString& currentVersion);

public slots:
    // Starts a request; finished() follows with the answer, or with an
    // invalid Result when the server could not be reached or understood.
    void run(const QUrl& endpoint);

signals:
    void finished(const UpdateCheck::Result& result);

private:
    QNetworkAccessManager* m_nam;
    bool m_busy = false;
};
