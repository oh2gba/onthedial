// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ScheduleSource.h"

// HFCC public data (http://www.hfcc.org/data/): <season>/<season>allx2.zip
// with the fixed-width schedule and reference tables.
class HfccSource : public ScheduleSource
{
    Q_OBJECT
public:
    HfccSource(StationDb* db, QNetworkAccessManager* nam, QObject* parent = nullptr);

public slots:
    void update() override;

private:
    void fetchZip(const QString& season, bool allowFallback);
};
