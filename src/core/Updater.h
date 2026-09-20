// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVector>

class ScheduleSource;

// Runs the enabled sources one after another, never in parallel.
class Updater : public QObject
{
    Q_OBJECT
public:
    explicit Updater(QObject* parent = nullptr);

    void addSource(ScheduleSource* source);
    const QVector<ScheduleSource*>& sources() const { return m_sources; }
    ScheduleSource* source(const QString& id) const;

    bool isBusy() const { return !m_queue.isEmpty() || m_current != nullptr; }
    bool anyStale(int maxAgeDays) const;

public slots:
    // Refresh enabled sources that are stale (or all enabled ones with force).
    void update(int maxAgeDays, bool force);

signals:
    void progress(const QString& message);
    void sourceFinished(ScheduleSource* source, bool ok, const QString& message);
    void finished(bool allOk, const QString& summary);

private:
    void next();

    QVector<ScheduleSource*> m_sources;
    QVector<ScheduleSource*> m_queue;
    ScheduleSource* m_current = nullptr;
    QStringList m_messages;
    bool m_allOk = true;
};
