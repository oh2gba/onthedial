// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ScheduleSource.h"

// EiBi (http://www.eibispace.de/dx/): sked-Xzz.csv plus README.TXT code
// tables. EiBi's README permits use of the lists in third-party software.
class EibiSource : public ScheduleSource
{
    Q_OBJECT
public:
    EibiSource(StationDb* db, QNetworkAccessManager* nam, QObject* parent = nullptr);

public slots:
    void update() override;

private:
    void fetchSchedule(const QString& season, bool allowFallback);
    void fetchReadme(const QString& season, int entryCount);
};
