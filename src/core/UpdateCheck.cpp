// SPDX-License-Identifier: GPL-3.0-or-later
#include "UpdateCheck.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrlQuery>

UpdateCheck::UpdateCheck(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
{
}

int UpdateCheck::compareVersions(const QString& a, const QString& b)
{
    static const QRegularExpression sep(QStringLiteral("[.\\-+]"));
    const QStringList pa = a.trimmed().split(sep, Qt::SkipEmptyParts);
    const QStringList pb = b.trimmed().split(sep, Qt::SkipEmptyParts);
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i)
    {
        // a missing part counts as 0, so "1.0" equals "1.0.0"
        bool okA = true, okB = true;
        const int va = pa.value(i).isEmpty() ? 0 : pa.value(i).toInt(&okA);
        const int vb = pb.value(i).isEmpty() ? 0 : pb.value(i).toInt(&okB);
        if (okA && okB)
        {
            if (va != vb)
                return va < vb ? -1 : 1;
            continue;
        }
        // "1.0.0-rc1" sorts before "1.0.0"; two suffixes compare as text
        if (okA != okB)
            return okA ? 1 : -1;
        const int c = pa.value(i).compare(pb.value(i), Qt::CaseInsensitive);
        if (c != 0)
            return c < 0 ? -1 : 1;
    }
    return 0;
}

QString UpdateCheck::platformName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    if (!qEnvironmentVariableIsEmpty("FLATPAK_ID"))
        return QStringLiteral("linux-flatpak");
    if (!qEnvironmentVariableIsEmpty("APPIMAGE"))
        return QStringLiteral("linux-appimage");
    return QStringLiteral("linux");
#endif
}

UpdateCheck::Result UpdateCheck::parseResponse(const QByteArray& json, const QString& currentVersion)
{
    Result r;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return r;
    const QJsonObject o = doc.object();
    r.latest = o.value(QStringLiteral("version")).toString().trimmed();
    r.url = o.value(QStringLiteral("url")).toString().trimmed();
    r.message = o.value(QStringLiteral("message")).toString().trimmed().left(200);
    static const QRegularExpression versionRe(QStringLiteral("^\\d+(\\.\\d+){0,3}([-+][0-9A-Za-z.]+)?$"));
    if (!versionRe.match(r.latest).hasMatch())
        return r;
    if (!r.url.isEmpty() && !r.url.startsWith(QLatin1String("https://")))
        r.url.clear();
    r.valid = true;
    r.newer = compareVersions(r.latest, currentVersion) > 0;
    return r;
}

void UpdateCheck::run(const QUrl& endpoint)
{
    if (m_busy || !endpoint.isValid())
        return;

    QUrl url = endpoint;
    QUrlQuery q(url);
    q.addQueryItem(QStringLiteral("v"), QCoreApplication::applicationVersion());
    q.addQueryItem(QStringLiteral("os"), platformName());
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("otd/%1 (%2)").arg(QCoreApplication::applicationVersion(), platformName()));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // Generous: on hosts whose IPv6 route is broken Qt only falls back to
    // IPv4 after the first attempt gives up, which takes a while.
    req.setTransferTimeout(90000);

    m_busy = true;
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_busy = false;
        if (reply->error() != QNetworkReply::NoError)
        {
            emit finished(Result());   // dead link or no network: never mind
            return;
        }
        emit finished(parseResponse(reply->readAll(), QCoreApplication::applicationVersion()));
    });
}
