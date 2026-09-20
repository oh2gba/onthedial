// SPDX-License-Identifier: GPL-3.0-or-later
#include "AokiParser.h"

#include <QRegularExpression>
#include <QStringDecoder>

namespace
{
struct Layout
{
    int station = -1, utc = -1, days = -1, lang = -1, pow = -1, azi = -1;
    int location = -1, adm = -1, latlon = -1, remarks = -1;
    bool valid() const { return station > 0 && utc > 0 && days > 0 && adm > 0; }
};

Layout layoutFromHeader(const QString& hdr)
{
    Layout l;
    l.station = hdr.indexOf(QLatin1String("STATION"));
    l.utc = hdr.indexOf(QLatin1String("UTC"));
    l.days = hdr.indexOf(QLatin1String("Su-W-Sa"));
    l.lang = hdr.indexOf(QLatin1String("Language"));
    l.pow = hdr.indexOf(QLatin1String("Pow"));
    l.azi = hdr.indexOf(QLatin1String("Azi"));
    l.location = hdr.indexOf(QLatin1String("Location"));
    l.adm = hdr.indexOf(QLatin1String("ADM"));
    l.latlon = hdr.indexOf(QLatin1String("L/L"));
    l.remarks = hdr.indexOf(QLatin1String("Remarks"));
    return l;
}

QString slice(const QString& s, int from, int to)
{
    if (from < 0 || from >= s.size())
        return QString();
    return (to < 0 ? s.mid(from) : s.mid(from, to - from)).trimmed();
}
} // namespace

AokiParser::ParseResult AokiParser::parse(const QByteArray& data)
{
    ParseResult result;
    QStringDecoder dec(QStringDecoder::Latin1);
    const QString text = dec(data);
    const QStringList lines = text.split(QLatin1Char('\n'));

    Layout layout;
    bool sundayFirst = true;
    static const QRegularExpression timeRe(QStringLiteral("^(\\d{4})-(\\d{4})$"));

    for (const QString& rawLine : lines)
    {
        QString raw = rawLine;
        if (raw.endsWith(QLatin1Char('\r')))
            raw.chop(1);
        if (raw.trimmed().isEmpty())
            continue;

        if (!layout.valid())
        {
            if (raw.startsWith(QLatin1String("FRE ")))
            {
                layout = layoutFromHeader(raw);
                continue;
            }
            if (result.title.isEmpty())
            {
                result.title = raw.trimmed();
                if (raw.contains(QLatin1String("Day 1 = Monday")))
                    sundayFirst = false;
            }
            continue;
        }

        // " 9500 CNR 1 Voice of China    ..." / "  765xYamaguchi Hoso ..."
        const QString freqField = raw.left(layout.station - 1);
        const QChar flag = raw.size() >= layout.station ? raw[layout.station - 1] : QLatin1Char(' ');
        bool ok = false;
        const double kHz = freqField.trimmed().toDouble(&ok);
        if (!ok || kHz <= 0.0)
        {
            ++result.skippedLines;
            continue;
        }

        const QString time = slice(raw, layout.utc, layout.days);
        const QRegularExpressionMatch tm = timeRe.match(time);
        if (!tm.hasMatch())
        {
            ++result.skippedLines;
            continue;
        }

        StationEntry e;
        e.source = QStringLiteral("aoki");
        e.kHz = kHz;
        const int s = tm.captured(1).toInt(), t = tm.captured(2).toInt();
        e.startMin = (s / 100) * 60 + s % 100;
        e.endMin = (t / 100) * 60 + t % 100;
        if (e.startMin == 0 && e.endMin == 0)
            e.endMin = 1440;

        // Days: "1234567" / ".234567" / "   1   ", day 1 = Sunday in Aoki files
        QString days;
        const QString dayField = slice(raw, layout.days, layout.lang);
        for (const QChar c : dayField)
        {
            if (!c.isDigit())
                continue;
            int d = c.digitValue();
            if (sundayFirst)
                d = (d == 1) ? 7 : d - 1;
            days.append(QChar(QLatin1Char('0' + d)));
        }
        if (days.size() == 7 || days.isEmpty())
            e.days.clear();
        else
        {
            QList<QChar> sorted;
            for (const QChar c : days)
                sorted.append(c);
            std::sort(sorted.begin(), sorted.end());
            for (const QChar c : sorted)
                e.days.append(c);
        }

        e.station = slice(raw, layout.station, layout.utc);
        e.langText = slice(raw, layout.lang, layout.pow);
        e.itu = slice(raw, layout.adm, layout.latlon);
        e.siteText = slice(raw, layout.location, layout.adm);

        QStringList remarks;
        const QString power = slice(raw, layout.pow, layout.azi);
        if (!power.isEmpty())
            remarks << power + QStringLiteral(" kW");
        const QString az = slice(raw, layout.azi, layout.location);
        if (!az.isEmpty() && az != QLatin1String("ND"))
            remarks << QStringLiteral("az %1°").arg(az);
        const QString rem = slice(raw, layout.remarks, -1);
        if (!rem.isEmpty())
            remarks << rem;
        if (flag == QLatin1Char('*'))
            remarks << QStringLiteral("*");
        e.remarks = remarks.join(QStringLiteral(", "));

        if (flag == QLatin1Char('x'))
            e.persistence = 8;   // marked off air

        const QString upperRemarks = rem.toUpper();
        if (e.langText.contains(QLatin1String("(Digital)")) || e.station.contains(QLatin1String("DRM"))
            || upperRemarks.contains(QLatin1String("DRM")))
            e.mode = QStringLiteral("DRM");
        else if (upperRemarks.contains(QLatin1String("USB")))
            e.mode = QStringLiteral("USB");
        else if (upperRemarks.contains(QLatin1String("LSB")))
            e.mode = QStringLiteral("LSB");
        else if (upperRemarks.contains(QLatin1String("CW")) || e.langText == QLatin1String("A1A"))
            e.mode = QStringLiteral("CW");
        else
            e.mode = QStringLiteral("AM");

        result.entries.push_back(e);
    }

    if (!layout.valid())
        result.error = QStringLiteral("header line not found");
    else if (result.entries.isEmpty())
        result.error = QStringLiteral("no schedule lines found");
    return result;
}
