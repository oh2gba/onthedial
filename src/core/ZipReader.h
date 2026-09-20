// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

// Minimal in-memory zip extraction on top of miniz.
namespace ZipReader
{
    // File name (as stored, case preserved) -> contents. Directories skipped.
    QHash<QString, QByteArray> extractAll(const QByteArray& zipData, QString* error = nullptr);

    // Convenience: first file whose name matches the case-insensitive regular
    // expression. Returns an empty QString when nothing matches.
    QString findName(const QHash<QString, QByteArray>& files, const QString& pattern);

    // Helper for tests: build a zip archive in memory.
    QByteArray create(const QHash<QString, QByteArray>& files);
}
