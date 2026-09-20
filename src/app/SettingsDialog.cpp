// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include "core/RigctldLauncher.h"
#include "core/StationDb.h"

#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QTabWidget>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
const char* kKeys[] = {"rig.host", "rig.port", "rig.pollMs", "view.toleranceKHz", "view.onAirOnly",
                       "view.followRig", "view.alwaysOnTop", "data.refreshDays", "data.eibiUrl",
                       "data.hfccUrl", "data.aokiUrl", "data.eibiEnabled", "data.hfccEnabled",
                       "data.aokiEnabled", "rigctld.launch", "rigctld.path", "rigctld.model",
                       "rigctld.device", "rigctld.baud", "rigctld.extra"};
QString key(int i) { return QStringLiteral("settings.") + QLatin1String(kKeys[i]); }
bool toBool(const QString& v, bool fallback)
{
    if (v.isEmpty()) return fallback;
    return v == QLatin1String("1") || v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}
} // namespace

void AppSettings::load(const StationDb* db)
{
    auto str = [db](int i, const QString& fallback) { return db->meta(key(i), fallback); };
    auto num = [&](int i, double fallback) {
        bool ok = false;
        const double v = str(i, QString()).toDouble(&ok);
        return ok ? v : fallback;
    };
    rigHost = str(0, rigHost);
    rigPort = int(num(1, rigPort));
    pollIntervalMs = int(num(2, pollIntervalMs));
    toleranceKHz = num(3, toleranceKHz);
    onAirOnly = toBool(str(4, QString()), onAirOnly);
    followRig = toBool(str(5, QString()), followRig);
    alwaysOnTop = toBool(str(6, QString()), alwaysOnTop);
    refreshDays = int(num(7, refreshDays));
    eibiUrl = str(8, eibiUrl);
    hfccUrl = str(9, hfccUrl);
    aokiUrl = str(10, aokiUrl);
    eibiEnabled = toBool(str(11, QString()), eibiEnabled);
    hfccEnabled = toBool(str(12, QString()), hfccEnabled);
    aokiEnabled = toBool(str(13, QString()), aokiEnabled);
    launchRigctld = toBool(str(14, QString()), launchRigctld);
    rigctldPath = str(15, rigctldPath);
    rigModel = int(num(16, rigModel));
    rigDevice = str(17, rigDevice);
    rigBaud = int(num(18, rigBaud));
    rigctldExtra = str(19, rigctldExtra);
}

void AppSettings::save(StationDb* db) const
{
    const QString values[] = {
        rigHost, QString::number(rigPort), QString::number(pollIntervalMs),
        QString::number(toleranceKHz), QString::number(onAirOnly ? 1 : 0),
        QString::number(followRig ? 1 : 0), QString::number(alwaysOnTop ? 1 : 0),
        QString::number(refreshDays), eibiUrl, hfccUrl, aokiUrl,
        QString::number(eibiEnabled ? 1 : 0), QString::number(hfccEnabled ? 1 : 0),
        QString::number(aokiEnabled ? 1 : 0), QString::number(launchRigctld ? 1 : 0),
        rigctldPath, QString::number(rigModel), rigDevice, QString::number(rigBaud),
        rigctldExtra};
    for (int i = 0; i < int(sizeof(kKeys) / sizeof(kKeys[0])); ++i)
        db->setMeta(key(i), values[i]);
}

SettingsDialog::SettingsDialog(const AppSettings& cur, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    auto* rigBox = new QGroupBox(tr("Connection to rigctld (or gqrx / SDR++ rigctl server)"));
    auto* rigForm = new QFormLayout(rigBox);
    m_host = new QLineEdit(cur.rigHost);
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(cur.rigPort);
    m_poll = new QSpinBox;
    m_poll->setRange(100, 10000);
    m_poll->setSingleStep(100);
    m_poll->setSuffix(tr(" ms"));
    m_poll->setValue(cur.pollIntervalMs);
    rigForm->addRow(tr("Host:"), m_host);
    rigForm->addRow(tr("Port:"), m_port);
    rigForm->addRow(tr("Poll interval:"), m_poll);

    // ---- start rigctld from here ------------------------------------
    auto* launchBox = new QGroupBox(tr("Start rigctld for me"));
    auto* launchForm = new QFormLayout(launchBox);
    m_launch = new QCheckBox(tr("Start Hamlib's rigctld when the program starts"));
    m_launch->setChecked(cur.launchRigctld);
    m_rigctldPath = new QLineEdit(cur.rigctldPath);
    m_rigctldPath->setPlaceholderText(RigctldLauncher::defaultPath());
    m_browse = new QPushButton(tr("..."));
    m_browse->setMaximumWidth(36);
    connect(m_browse, &QPushButton::clicked, this, &SettingsDialog::browseRigctld);
    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(m_rigctldPath, 1);
    pathRow->addWidget(m_browse);
    m_model = new QComboBox;
    m_model->setEditable(false);
    m_model->setMinimumWidth(340);
    m_currentModel = cur.rigModel;
    auto* reload = new QPushButton(tr("Reload list"));
    connect(reload, &QPushButton::clicked, this, &SettingsDialog::reloadModels);
    auto* modelRow = new QHBoxLayout;
    modelRow->addWidget(m_model, 1);
    modelRow->addWidget(reload);
    m_device = new QComboBox;
    m_device->setEditable(true);
    m_device->addItems(RigctldLauncher::serialPortCandidates());
    m_device->setCurrentText(cur.rigDevice);
    m_device->lineEdit()->setPlaceholderText(tr("/dev/ttyUSB0, COM3, or host:port"));
    m_baud = new QComboBox;
    m_baud->addItem(tr("rig default"), 0);
    for (int b : {4800, 9600, 19200, 38400, 57600, 115200})
        m_baud->addItem(QString::number(b), b);
    m_baud->setCurrentIndex(qMax(0, m_baud->findData(cur.rigBaud)));
    m_extra = new QLineEdit(cur.rigctldExtra);
    m_extra->setPlaceholderText(tr("e.g. --set-conf=stop_bits=2 --civaddr=0x94"));
    m_launchNote = new QLabel;
    m_launchNote->setWordWrap(true);
    launchForm->addRow(m_launch);
    launchForm->addRow(tr("rigctld program:"), pathRow);
    launchForm->addRow(tr("Rig model:"), modelRow);
    launchForm->addRow(tr("Device:"), m_device);
    launchForm->addRow(tr("Baud rate:"), m_baud);
    launchForm->addRow(tr("Extra arguments:"), m_extra);
    launchForm->addRow(m_launchNote);
    auto enableLaunch = [this](bool on) {
        for (QWidget* w : std::initializer_list<QWidget*>{m_rigctldPath, m_browse, m_model, m_device, m_baud, m_extra})
            w->setEnabled(on);
    };
    enableLaunch(cur.launchRigctld);
    connect(m_launch, &QCheckBox::toggled, this, enableLaunch);
    reloadModels();

    auto* viewBox = new QGroupBox(tr("Display"));
    auto* viewForm = new QFormLayout(viewBox);
    m_tolerance = new QDoubleSpinBox;
    m_tolerance->setRange(0.1, 500.0);
    m_tolerance->setDecimals(1);
    m_tolerance->setSuffix(tr(" kHz"));
    m_tolerance->setValue(cur.toleranceKHz);
    viewForm->addRow(tr("Default search width (±):"), m_tolerance);

    auto* dataBox = new QGroupBox(tr("Databases"));
    auto* dataForm = new QFormLayout(dataBox);
    m_refreshDays = new QSpinBox;
    m_refreshDays->setRange(1, 90);
    m_refreshDays->setSuffix(tr(" days"));
    m_refreshDays->setValue(cur.refreshDays);
    dataForm->addRow(tr("Refresh when older than:"), m_refreshDays);

    auto addSource = [&](const QString& label, bool enabled, const QString& url,
                         QCheckBox** box, QLineEdit** edit) {
        *box = new QCheckBox(label);
        (*box)->setChecked(enabled);
        *edit = new QLineEdit(url);
        (*edit)->setMinimumWidth(340);
        (*edit)->setEnabled(enabled);
        connect(*box, &QCheckBox::toggled, *edit, &QLineEdit::setEnabled);
        dataForm->addRow(*box, *edit);
    };
    addSource(tr("EiBi"), cur.eibiEnabled, cur.eibiUrl, &m_eibiOn, &m_eibiUrl);
    addSource(tr("HFCC"), cur.hfccEnabled, cur.hfccUrl, &m_hfccOn, &m_hfccUrl);
    addSource(tr("Aoki"), cur.aokiEnabled, cur.aokiUrl, &m_aokiOn, &m_aokiUrl);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* rigTab = new QWidget;
    auto* rigLayout = new QVBoxLayout(rigTab);
    rigLayout->addWidget(rigBox);
    rigLayout->addWidget(launchBox);
    rigLayout->addStretch();
    auto* dataTab = new QWidget;
    auto* dataLayout = new QVBoxLayout(dataTab);
    dataLayout->addWidget(viewBox);
    dataLayout->addWidget(dataBox);
    dataLayout->addStretch();
    auto* tabs = new QTabWidget;
    tabs->addTab(rigTab, tr("Radio"));
    tabs->addTab(dataTab, tr("Display and data"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

void SettingsDialog::reloadModels()
{
    QString error;
    const QVector<RigctldLauncher::Model> models =
        RigctldLauncher::listModels(m_rigctldPath->text(), &error);
    m_model->clear();
    int selected = -1;
    for (const RigctldLauncher::Model& m : models)
    {
        m_model->addItem(m.label(), m.number);
        if (m.number == m_currentModel)
            selected = m_model->count() - 1;
    }
    if (models.isEmpty())
    {
        m_model->addItem(tr("%1  (rigctld not found, keeping model number)").arg(m_currentModel),
                         m_currentModel);
        m_launchNote->setText(tr("Could not run rigctld: %1. Install Hamlib or point to the "
                                 "program above.").arg(error));
    }
    else
    {
        m_launchNote->setText(tr("%1 rig models known to this rigctld. Pick your radio, the port "
                                 "it is connected to and, if needed, the baud rate set in the radio's menu.")
                                 .arg(models.size()));
        if (selected < 0)
            selected = m_model->findData(1);
    }
    m_model->setCurrentIndex(qMax(0, selected));
}

void SettingsDialog::browseRigctld()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Locate rigctld"));
    if (!file.isEmpty())
    {
        m_rigctldPath->setText(file);
        reloadModels();
    }
}

AppSettings SettingsDialog::settings(const AppSettings& base) const
{
    AppSettings s = base;
    s.rigHost = m_host->text().trimmed().isEmpty() ? QStringLiteral("localhost")
                                                   : m_host->text().trimmed();
    s.rigPort = m_port->value();
    s.pollIntervalMs = m_poll->value();
    s.toleranceKHz = m_tolerance->value();
    s.refreshDays = m_refreshDays->value();
    s.eibiUrl = m_eibiUrl->text().trimmed();
    s.hfccUrl = m_hfccUrl->text().trimmed();
    s.aokiUrl = m_aokiUrl->text().trimmed();
    s.eibiEnabled = m_eibiOn->isChecked();
    s.hfccEnabled = m_hfccOn->isChecked();
    s.aokiEnabled = m_aokiOn->isChecked();
    s.launchRigctld = m_launch->isChecked();
    s.rigctldPath = m_rigctldPath->text().trimmed();
    s.rigModel = m_model->currentData().toInt();
    s.rigDevice = m_device->currentText().trimmed();
    s.rigBaud = m_baud->currentData().toInt();
    s.rigctldExtra = m_extra->text().trimmed();
    return s;
}
