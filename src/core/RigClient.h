// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QQueue>
#include <QTcpSocket>
#include <QTimer>

// Polls a rigctl-protocol server (Hamlib rigctld on port 4532, gqrx, SDR++)
// for VFO frequency and mode. Reconnects automatically.
class RigClient : public QObject
{
    Q_OBJECT
public:
    explicit RigClient(QObject* parent = nullptr);

    void setEndpoint(const QString& host, quint16 port);
    void setPollInterval(int ms);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    bool isConnected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }
    qint64 frequencyHz() const { return m_frequencyHz; }
    QString mode() const { return m_mode; }
    int passbandHz() const { return m_passband; }

public slots:
    void start();
    void stop();
    // Tune the rig ("F"/"M" commands); no-ops when disconnected.
    void setFrequency(qint64 hz);
    void setMode(const QString& mode);
    // Drop the connection and retry shortly (e.g. after rigctld was started).
    void reconnectSoon();

signals:
    void frequencyChanged(qint64 hz);
    void modeChanged(const QString& mode, int passbandHz);
    void stateChanged(bool connected, const QString& message);

private slots:
    void connectNow();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void poll();

private:
    void handleLine(const QByteArray& line);
    void scheduleReconnect();

    enum Expect { ExFreq, ExMode, ExPassband, ExReport };
    QQueue<Expect> m_expect;   // answers still owed by the server, in order

    QTcpSocket m_socket;
    QTimer m_pollTimer;
    QTimer m_reconnectTimer;
    QByteArray m_buffer;
    QString m_host = QStringLiteral("localhost");
    quint16 m_port = 4532;
    bool m_enabled = false;
    int m_missedPolls = 0;
    qint64 m_frequencyHz = 0;
    QString m_mode;
    int m_passband = 0;
};
