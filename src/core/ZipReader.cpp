// SPDX-License-Identifier: GPL-3.0-or-later
#include "ZipReader.h"

#include <QRegularExpression>
#include <cstring>

#include "miniz.h"

QHash<QString, QByteArray> ZipReader::extractAll(const QByteArray& zipData, QString* error)
{
    QHash<QString, QByteArray> out;
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, zipData.constData(), size_t(zipData.size()), 0))
    {
        if (error)
            *error = QStringLiteral("not a zip archive (%1)")
                         .arg(QString::fromLatin1(mz_zip_get_error_string(mz_zip_get_last_error(&zip))));
        return out;
    }

    const mz_uint n = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < n; ++i)
    {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
            continue;
        size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (!data)
        {
            if (error)
                *error = QStringLiteral("cannot extract %1").arg(QString::fromUtf8(st.m_filename));
            continue;
        }
        out.insert(QString::fromUtf8(st.m_filename),
                   QByteArray(static_cast<const char*>(data), int(size)));
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    return out;
}

QString ZipReader::findName(const QHash<QString, QByteArray>& files, const QString& pattern)
{
    const QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
    QStringList names = files.keys();
    names.sort();
    for (const QString& name : names)
        if (re.match(name).hasMatch())
            return name;
    return QString();
}

QByteArray ZipReader::create(const QHash<QString, QByteArray>& files)
{
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0))
        return QByteArray();
    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        const QByteArray name = it.key().toUtf8();
        mz_zip_writer_add_mem(&zip, name.constData(), it.value().constData(),
                              size_t(it.value().size()), MZ_DEFAULT_COMPRESSION);
    }
    void* buf = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &buf, &size);
    QByteArray out(static_cast<const char*>(buf), int(size));
    mz_free(buf);
    mz_zip_writer_end(&zip);
    return out;
}
