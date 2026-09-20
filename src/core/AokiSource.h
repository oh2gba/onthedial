// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ScheduleSource.h"

// Aoki / Bi Newsletter list (NDXC, http://www1.s2.starcat.ne.jp/ndxc/).
// The season zip lives in a changing sub-directory, so the index page is
// read first to find the link "...<xx><season>.zip".
class AokiSource : public ScheduleSource
{
    Q_OBJECT
public:
    AokiSource(StationDb* db, QNetworkAccessManager* nam, QObject* parent = nullptr);

    // Extracts the zip link for a season from the index page HTML.
    static QUrl findZipLink(const QByteArray& html, const QString& season, const QUrl& base);

public slots:
    void update() override;

private:
    void fetchIndex();
    void fetchZip(const QUrl& url, const QString& season);
};
