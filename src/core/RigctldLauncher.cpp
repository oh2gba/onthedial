// SPDX-License-Identifier: GPL-3.0-or-later
#include "RigctldLauncher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

QString RigctldLauncher::Model::label() const
{
    return QStringLiteral("%1  %2 %3").arg(number).arg(manufacturer, name);
}

RigctldLauncher::RigctldLauncher(QObject* parent)
    : QObject(parent)
{
}

RigctldLauncher::~RigctldLauncher()
{
    stop();
}

QString RigctldLauncher::defaultPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString bundled = appDir + QStringLiteral("/hamlib/rigctld.exe");
    const QString exe = QStringLiteral("rigctld.exe");
#else
    const QString bundled = appDir + QStringLiteral("/hamlib/rigctld");
    const QString exe = QStringLiteral("rigctld");
#endif
    if (QFileInfo::exists(bundled))
        return bundled;
    QString inPath = QStandardPaths::findExecutable(exe);
    if (inPath.isEmpty())
    {
        // GUI apps on macOS do not see Homebrew's PATH; Flatpak keeps it in /app.
        const QStringList extra = {QStringLiteral("/opt/homebrew/bin"), QStringLiteral("/usr/local/bin"),
                                   QStringLiteral("/app/bin")};
        inPath = QStandardPaths::findExecutable(exe, extra);
    }
    return inPath.isEmpty() ? exe : inPath;
}

QString RigctldLauncher::resolvePath(const QString& configured)
{
    return configured.trimmed().isEmpty() ? defaultPath() : configured.trimmed();
}

QVector<RigctldLauncher::Model> RigctldLauncher::listModels(const QString& path, QString* error)
{
    QVector<Model> models;
    QProcess p;
    p.start(resolvePath(path), {QStringLiteral("-l")});
    if (!p.waitForStarted(3000) || !p.waitForFinished(15000))
    {
        if (error)
            *error = p.errorString();
        return models;
    }
    // "  1     Hamlib                 Dummy                   20250718.0      Stable      RIG_MODEL_DUMMY"
    static const QRegularExpression re(
        QStringLiteral("^\\s*(\\d+)\\s+(\\S.*?)\\s{2,}(\\S.*?)\\s{2,}(\\S+)\\s+(\\S+)"));
    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    for (const QString& line : out.split(QLatin1Char('\n')))
    {
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch())
            continue;
        Model model;
        model.number = m.captured(1).toInt();
        model.manufacturer = m.captured(2).trimmed();
        model.name = m.captured(3).trimmed();
        model.status = m.captured(5).trimmed();
        models.push_back(model);
    }
    std::sort(models.begin(), models.end(), [](const Model& a, const Model& b) {
        const int c = a.manufacturer.compare(b.manufacturer, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    if (models.isEmpty() && error)
        *error = QObject::tr("no rig models found in the output of rigctld -l");
    return models;
}

QStringList RigctldLauncher::buildArguments(const Config& cfg)
{
    QStringList args;
    args << QStringLiteral("-m") << QString::number(cfg.model);
    if (!cfg.device.trimmed().isEmpty())
        args << QStringLiteral("-r") << cfg.device.trimmed();
    if (cfg.baud > 0)
        args << QStringLiteral("-s") << QString::number(cfg.baud);
    args << QStringLiteral("-t") << QString::number(cfg.port);
    if (!cfg.extraArgs.trimmed().isEmpty())
        args << QProcess::splitCommand(cfg.extraArgs.trimmed());
    return args;
}

QStringList RigctldLauncher::serialPortCandidates()
{
    QStringList out;
#ifdef Q_OS_WIN
    for (int i = 1; i <= 32; ++i)
        out << QStringLiteral("COM%1").arg(i);
#else
    const QDir dev(QStringLiteral("/dev"));
    const QStringList patterns = {QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*"),
                                  QStringLiteral("ttyS[0-3]"), QStringLiteral("cu.usb*"),
                                  QStringLiteral("cu.SLAB*"), QStringLiteral("cu.wchusb*")};
    for (const QString& name : dev.entryList(patterns, QDir::System | QDir::Files, QDir::Name))
        out << dev.filePath(name);
    const QDir byId(QStringLiteral("/dev/serial/by-id"));
    for (const QString& name : byId.entryList(QDir::Files | QDir::System, QDir::Name))
        out << byId.filePath(name);
#endif
    return out;
}

bool RigctldLauncher::start(const Config& cfg)
{
    stop();
    m_lastError.clear();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::started, this, &RigctldLauncher::started);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process)
            m_lastError = m_process->errorString();
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                QString output;
                if (m_process)
                    output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
                const QString msg = m_stopping
                    ? tr("rigctld stopped")
                    : tr("rigctld exited (%1)%2").arg(status == QProcess::CrashExit ? tr("crashed")
                                                                                     : QString::number(code),
                                                      output.isEmpty() ? QString() : QStringLiteral(": ") + output.section(QLatin1Char('\n'), -1));
                m_stopping = false;
                emit stopped(msg);
            });

    const QString exe = resolvePath(cfg.path);
    m_process->start(exe, buildArguments(cfg));
    if (!m_process->waitForStarted(5000))
    {
        m_lastError = tr("cannot start %1: %2").arg(exe, m_process->errorString());
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }
    return true;
}

void RigctldLauncher::stop()
{
    if (!m_process)
        return;
    QProcess* p = m_process;
    m_process = nullptr;
    if (p->state() != QProcess::NotRunning)
    {
        m_stopping = true;
        p->terminate();
        if (!p->waitForFinished(3000))
        {
            p->kill();
            p->waitForFinished(1000);
        }
    }
    p->deleteLater();
}

bool RigctldLauncher::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}
