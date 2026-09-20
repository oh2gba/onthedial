// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>

// Links into the Signal Identification Wiki (sigidwiki.com), which has
// sound samples and waterfall pictures for the modes we list.
namespace SigidWiki
{
    // Page for a mode from the Mode column ("HFDL", "RTTY", ...); empty when unknown.
    QUrl modeUrl(const QString& mode);
    // Full-text search on the wiki, e.g. for a station name.
    QUrl searchUrl(const QString& text);
    QUrl homeUrl();
}
