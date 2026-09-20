// SPDX-License-Identifier: GPL-3.0-or-later
#include "SigidWiki.h"

#include <QHash>
#include <QUrlQuery>

namespace
{
const QString kBase = QStringLiteral("https://www.sigidwiki.com/");
}

QUrl SigidWiki::modeUrl(const QString& mode)
{
    // Titles verified against the wiki's search API, September 2026.
    static const QHash<QString, QString> pages = {
        {QStringLiteral("HFDL"), QStringLiteral("High_Frequency_Data_Link_(HFDL)")},
        {QStringLiteral("RTTY"), QStringLiteral("Radio_Teletype_(RTTY)")},
        {QStringLiteral("FAX"),  QStringLiteral("Radiofax")},
        {QStringLiteral("CW"),   QStringLiteral("Morse_Code_(CW)")},
        {QStringLiteral("DRM"),  QStringLiteral("Digital_Radio_Mondiale_(DRM)")},
        {QStringLiteral("AM"),   QStringLiteral("Amplitude_Modulation_(AM)")},
        {QStringLiteral("USB"),  QStringLiteral("Single_Sideband_Voice")},
        {QStringLiteral("LSB"),  QStringLiteral("Single_Sideband_Voice")},
        {QStringLiteral("ALE"),  QStringLiteral("Automatic_Link_Establishment_(ALE)")},
        {QStringLiteral("OTH"),  QStringLiteral("Over_the_Horizon_Radar_(OTH)")},
        {QStringLiteral("STANAG"), QStringLiteral("STANAG_4285")},
    };
    const QString page = pages.value(mode.trimmed().toUpper());
    if (page.isEmpty())
        return QUrl();
    return QUrl(kBase + QStringLiteral("wiki/") + page);
}

QUrl SigidWiki::searchUrl(const QString& text)
{
    QUrl url(kBase + QStringLiteral("index.php"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("search"), text.trimmed());
    url.setQuery(q);
    return url;
}

QUrl SigidWiki::homeUrl()
{
    return QUrl(kBase);
}
