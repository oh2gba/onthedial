// SPDX-License-Identifier: GPL-3.0-or-later
#include "BandPlan.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

// The band plan lives in the static core library; make sure its resource
// object is linked in.
static void initCoreResources()
{
    Q_INIT_RESOURCE(core);
}

BandPlan::BandPlan() = default;

bool BandPlan::loadJson(const QByteArray& json, QString* error)
{
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (doc.isNull())
    {
        if (error)
            *error = perr.errorString();
        return false;
    }
    QVector<Band> bands;
    const QJsonArray arr = doc.object().value(QStringLiteral("bands")).toArray();
    for (const QJsonValue& v : arr)
    {
        const QJsonObject o = v.toObject();
        Band b;
        b.fromKHz = o.value(QStringLiteral("from")).toDouble();
        b.toKHz = o.value(QStringLiteral("to")).toDouble();
        b.kind = o.value(QStringLiteral("kind")).toString();
        b.name = o.value(QStringLiteral("name")).toString();
        for (const QJsonValue& r : o.value(QStringLiteral("regions")).toArray())
            b.regions.push_back(r.toInt());
        if (b.toKHz > b.fromKHz && !b.name.isEmpty())
            bands.push_back(b);
    }
    if (bands.isEmpty())
    {
        if (error)
            *error = QStringLiteral("no bands found");
        return false;
    }
    m_bands = bands;
    return true;
}

BandPlan BandPlan::builtIn()
{
    initCoreResources();
    BandPlan plan;
    QFile f(QStringLiteral(":/bandplan.json"));
    if (f.open(QIODevice::ReadOnly))
        plan.loadJson(f.readAll());
    return plan;
}

QVector<BandPlan::Band> BandPlan::lookup(double kHz, int region) const
{
    QVector<Band> out;
    for (const Band& b : m_bands)
        if (kHz >= b.fromKHz && kHz <= b.toKHz && b.appliesTo(region))
            out.push_back(b);
    return out;
}

QString BandPlan::describe(double kHz, int region) const
{
    QStringList names;
    for (const Band& b : lookup(kHz, region))
        names << b.name;
    return names.join(QStringLiteral(" · "));
}
