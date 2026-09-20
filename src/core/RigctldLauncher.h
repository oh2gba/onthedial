// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVector>

// Starts Hamlib's rigctld for the user, so nothing has to be typed into a
// terminal. Mirrors what QLog's "share rig" option does.
class RigctldLauncher : public QObject
{
    Q_OBJECT
public:
    struct Model
    {
        int number = 0;
        QString manufacturer;
        QString name;
        QString status;
        QString label() const;
    };

    struct Config
    {
        bool enabled = false;
        QString path;          // rigctld executable, empty = auto
        int model = 1;         // Hamlib model number (1 = dummy)
        QString device;        // /dev/ttyUSB0, COM3, or host:port for network rigs
        int baud = 0;          // 0 = Hamlib default for the rig
        QString extraArgs;     // anything else, e.g. "--set-conf=stop_bits=2"
        quint16 port = 4532;
    };

    explicit RigctldLauncher(QObject* parent = nullptr);
    ~RigctldLauncher() override;

    // Bundled copy next to the executable (Windows zip) or "rigctld" in PATH.
    static QString defaultPath();
    static QString resolvePath(const QString& configured);
    // Runs "rigctld -l" and parses the model table. Empty when it fails.
    static QVector<Model> listModels(const QString& path, QString* error = nullptr);
    static QStringList buildArguments(const Config& cfg);
    static QStringList serialPortCandidates();

    bool start(const Config& cfg);
    void stop();
    bool isRunning() const;
    QString lastError() const { return m_lastError; }

signals:
    void started();
    void stopped(const QString& message);

private:
    QProcess* m_process = nullptr;
    QString m_lastError;
    bool m_stopping = false;
};
