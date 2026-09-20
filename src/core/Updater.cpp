// SPDX-License-Identifier: GPL-3.0-or-later
#include "Updater.h"
#include "ScheduleSource.h"

Updater::Updater(QObject* parent)
    : QObject(parent)
{
}

void Updater::addSource(ScheduleSource* source)
{
    m_sources.push_back(source);
    connect(source, &ScheduleSource::progress, this, &Updater::progress);
    connect(source, &ScheduleSource::finished, this, [this, source](bool ok, const QString& msg) {
        if (source != m_current)
            return;
        m_current = nullptr;
        m_allOk = m_allOk && ok;
        m_messages << msg;
        emit sourceFinished(source, ok, msg);
        next();
    });
}

ScheduleSource* Updater::source(const QString& id) const
{
    for (ScheduleSource* s : m_sources)
        if (s->id() == id)
            return s;
    return nullptr;
}

bool Updater::anyStale(int maxAgeDays) const
{
    for (ScheduleSource* s : m_sources)
        if (s->isEnabled() && s->isStale(maxAgeDays))
            return true;
    return false;
}

void Updater::update(int maxAgeDays, bool force)
{
    if (isBusy())
        return;
    m_messages.clear();
    m_allOk = true;
    for (ScheduleSource* s : m_sources)
        if (s->isEnabled() && (force || s->isStale(maxAgeDays)))
            m_queue.push_back(s);
    if (m_queue.isEmpty())
    {
        emit finished(true, tr("All databases are current"));
        return;
    }
    next();
}

void Updater::next()
{
    if (m_queue.isEmpty())
    {
        emit finished(m_allOk, m_messages.join(QStringLiteral(" | ")));
        return;
    }
    m_current = m_queue.takeFirst();
    m_current->update();
}
