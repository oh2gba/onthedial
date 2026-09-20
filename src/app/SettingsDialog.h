// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class StationDb;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;

struct AppSettings
{
    QString rigHost = QStringLiteral("localhost");
    int rigPort = 4532;
    int pollIntervalMs = 500;
    double toleranceKHz = 5.0;
    int refreshDays = 7;
    QString eibiUrl = QStringLiteral("http://www.eibispace.de/dx/");
    QString hfccUrl = QStringLiteral("http://www.hfcc.org/data/");
    QString aokiUrl = QStringLiteral("http://www1.s2.starcat.ne.jp/ndxc/");
    bool eibiEnabled = true;
    bool hfccEnabled = true;
    bool aokiEnabled = true;
    // "start rigctld for me"
    bool launchRigctld = false;
    QString rigctldPath;
    int rigModel = 1;
    QString rigDevice;
    int rigBaud = 0;
    QString rigctldExtra;
    bool onAirOnly = false;
    bool followRig = true;
    bool alwaysOnTop = false;

    void load(const StationDb* db);
    void save(StationDb* db) const;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings& current, QWidget* parent = nullptr);
    // Returns a copy of the given settings with the dialog fields applied.
    AppSettings settings(const AppSettings& base) const;

private:
    QLineEdit* m_host;
    QSpinBox* m_port;
    QSpinBox* m_poll;
    QDoubleSpinBox* m_tolerance;
    QSpinBox* m_refreshDays;
    QCheckBox* m_eibiOn;
    QCheckBox* m_hfccOn;
    QCheckBox* m_aokiOn;
    QLineEdit* m_eibiUrl;
    QLineEdit* m_hfccUrl;
    QLineEdit* m_aokiUrl;

    QCheckBox* m_launch;
    QLineEdit* m_rigctldPath;
    QPushButton* m_browse;
    QComboBox* m_model;
    QComboBox* m_device;
    QComboBox* m_baud;
    QLineEdit* m_extra;
    QLabel* m_launchNote;
    int m_currentModel = 1;

private slots:
    void reloadModels();
    void browseRigctld();
};
