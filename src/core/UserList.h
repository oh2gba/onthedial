// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Station.h"
#include <QByteArray>

// Import/export of the personal station list as a semicolon separated
// text file (UTF-8), one entry per line:
//   kHz;Start;End;Days;Station;Mode;Country;Language;Site;Notes
namespace UserList
{
    QByteArray toCsv(const StationList& entries);
    StationList fromCsv(const QByteArray& data, int* skipped = nullptr);
}
