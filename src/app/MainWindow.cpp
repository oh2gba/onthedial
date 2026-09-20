// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "MyStationsDialog.h"
#include "StationEditDialog.h"
#include "StationModel.h"
#include "core/SigidWiki.h"
#include "core/AokiSource.h"
#include "core/EibiSource.h"
#include "core/HfccSource.h"
#include "core/RigClient.h"
#include "core/RigctldLauncher.h"
#include "core/StationDb.h"
#include "core/Updater.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QMenu>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

// Text filter over all columns plus an optional "on air only" gate.
class StationFilter : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setOnAirOnly(bool on)
    {
        m_onAirOnly = on;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        if (m_onAirOnly)
        {
            const QModelIndex idx = sourceModel()->index(row, 0, parent);
            const int rank = sourceModel()->data(idx, StationModel::OnAirRankRole).toInt();
            if (rank > 1)   // keep "on air" and "maybe"
                return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }

private:
    bool m_onAirOnly = false;
};

MainWindow::MainWindow(const QString& dataDir, double startKHz, QWidget* parent)
    : QMainWindow(parent)
    , m_dataDir(dataDir)
{
    m_db = new StationDb(m_dataDir + QStringLiteral("/stations.db"), this);
    if (!m_db->open())
        QMessageBox::critical(this, tr("Database error"),
                              tr("Cannot open %1:\n%2").arg(m_db->filePath(), m_db->lastError()));
    m_settings.load(m_db);

    m_nam = new QNetworkAccessManager(this);
    m_updater = new Updater(this);
    m_updater->addSource(new EibiSource(m_db, m_nam, this));
    m_updater->addSource(new HfccSource(m_db, m_nam, this));
    m_updater->addSource(new AokiSource(m_db, m_nam, this));
    m_rig = new RigClient(this);
    m_launcher = new RigctldLauncher(this);
    connect(m_launcher, &RigctldLauncher::started, this, [this]() {
        statusBar()->showMessage(tr("rigctld started"), 5000);
        m_rig->reconnectSoon();
    });
    connect(m_launcher, &RigctldLauncher::stopped, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 15000);
    });
    m_model = new StationModel(m_db, this);
    m_proxy = new StationFilter(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(StationModel::SortRole);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);

    buildUi();
    applySettings();

    connect(m_rig, &RigClient::frequencyChanged, this, &MainWindow::onRigFrequency);
    connect(m_rig, &RigClient::modeChanged, this, &MainWindow::onRigMode);
    connect(m_rig, &RigClient::stateChanged, this, &MainWindow::onRigState);
    connect(m_updater, &Updater::progress, this, [this](const QString& m) {
        statusBar()->showMessage(m);
    });
    connect(m_updater, &Updater::sourceFinished, this,
            [this](ScheduleSource*, bool, const QString&) { updateDbStatus(); });
    connect(m_updater, &Updater::finished, this, &MainWindow::onUpdateFinished);
    connect(m_db, &StationDb::changed, this, &MainWindow::refreshLookup);

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, &MainWindow::tick);
    m_tick->start();
    tick();

    restoreGeometry(QByteArray::fromBase64(
        m_db->meta(QStringLiteral("window.geometry")).toLatin1()));
    m_table->horizontalHeader()->restoreState(QByteArray::fromBase64(
        m_db->meta(QStringLiteral("window.columns")).toLatin1()));
    // The saved state also carries resize modes; a state written by an
    // older build had the Station column locked to "stretch". Re-assert
    // that the user may drag every column.
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(false);

    if (startKHz > 0.0)
    {
        // Manual start for this session only; the stored "Follow rig"
        // preference is left untouched.
        const QSignalBlocker b(m_followRig);
        m_followRig->setChecked(false);
        m_freqEdit->setReadOnly(false);
        m_freqEdit->setText(QString::number(startKHz, 'f', 3));
        setCentreKHz(startKHz, false);
    }

    updateDbStatus();
    if (m_updater->anyStale(m_settings.refreshDays))
        QTimer::singleShot(0, this, [this]() {
            m_updateAction->setEnabled(false);
            m_updater->update(m_settings.refreshDays, false);
        });

    applyLauncher();
    m_rig->start();
}

MainWindow::~MainWindow() = default;

void MainWindow::applyLauncher()
{
    if (!m_settings.launchRigctld)
    {
        m_launcher->stop();
        return;
    }
    RigctldLauncher::Config cfg;
    cfg.enabled = true;
    cfg.path = m_settings.rigctldPath;
    cfg.model = m_settings.rigModel;
    cfg.device = m_settings.rigDevice;
    cfg.baud = m_settings.rigBaud;
    cfg.extraArgs = m_settings.rigctldExtra;
    cfg.port = quint16(m_settings.rigPort);
    if (!m_launcher->start(cfg))
        QMessageBox::warning(this, tr("rigctld"),
                             tr("Could not start rigctld:\n%1\n\nCheck File > Settings > Radio.")
                                 .arg(m_launcher->lastError()));
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("On The Dial"));
    setWindowIcon(QIcon(QStringLiteral(":/otd.svg")));

    // --- menu ---------------------------------------------------------
    QMenu* file = menuBar()->addMenu(tr("&File"));
    m_updateAction = file->addAction(tr("&Update databases now"), this, &MainWindow::updateDatabases);
    file->addAction(tr("&Settings..."), this, &MainWindow::openSettings);
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    QMenu* stations = menuBar()->addMenu(tr("&Stations"));
    stations->addAction(tr("&Add station at this frequency..."), QKeySequence(Qt::CTRL | Qt::Key_N),
                        this, &MainWindow::addMyStation);
    stations->addAction(tr("&My stations..."), QKeySequence(Qt::CTRL | Qt::Key_M),
                        this, &MainWindow::openMyStations);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    m_onTopAction = view->addAction(tr("Always on &top"));
    m_onTopAction->setCheckable(true);
    connect(m_onTopAction, &QAction::toggled, this, &MainWindow::onAlwaysOnTopToggled);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&Connecting your radio (web)"), this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://onthedial.oh2gba.eu/rig.html")));
    });
    help->addAction(tr("&Signal Identification Wiki (web)"), this, []() {
        QDesktopServices::openUrl(SigidWiki::homeUrl());
    });
    help->addSeparator();
    help->addAction(tr("&About On The Dial"), this, &MainWindow::about);

    // --- header -------------------------------------------------------
    m_freqLabel = new QLabel(QStringLiteral("---.--- kHz"));
    QFont big = m_freqLabel->font();
    big.setPointSize(big.pointSize() * 2 + 4);
    big.setBold(true);
    big.setFamily(QStringLiteral("monospace"));
    big.setStyleHint(QFont::Monospace);
    m_freqLabel->setFont(big);

    m_modeLabel = new QLabel;
    QFont mid = m_modeLabel->font();
    mid.setPointSize(mid.pointSize() + 3);
    m_modeLabel->setFont(mid);

    m_clockLabel = new QLabel;
    m_clockLabel->setFont(mid);
    m_clockLabel->setToolTip(tr("Current UTC time; schedules are evaluated against it"));

    auto* header = new QHBoxLayout;
    header->addWidget(m_freqLabel);
    header->addSpacing(12);
    header->addWidget(m_modeLabel);
    header->addStretch();
    header->addWidget(m_clockLabel);

    // --- controls -----------------------------------------------------
    m_followRig = new QCheckBox(tr("Follow rig"));
    m_followRig->setToolTip(tr("Track the VFO of rigctld. Untick to type a frequency yourself."));
    connect(m_followRig, &QCheckBox::toggled, this, &MainWindow::onFollowToggled);

    m_freqEdit = new QLineEdit;
    m_freqEdit->setPlaceholderText(tr("kHz"));
    m_freqEdit->setMaximumWidth(110);
    m_freqEdit->setValidator(new QDoubleValidator(0.0, 100000.0, 3, m_freqEdit));
    connect(m_freqEdit, &QLineEdit::editingFinished, this, &MainWindow::onFrequencyEdited);

    m_tolerance = new QDoubleSpinBox;
    m_tolerance->setRange(0.1, 500.0);
    m_tolerance->setDecimals(1);
    m_tolerance->setSingleStep(1.0);
    m_tolerance->setPrefix(QStringLiteral("± "));
    m_tolerance->setSuffix(tr(" kHz"));
    m_tolerance->setToolTip(tr("Show entries this close to the tuned frequency"));
    connect(m_tolerance, &QDoubleSpinBox::valueChanged, this, &MainWindow::onToleranceChanged);

    m_onAirOnly = new QCheckBox(tr("On air only"));
    connect(m_onAirOnly, &QCheckBox::toggled, this, &MainWindow::onOnAirOnlyToggled);

    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Search all stations (name, language, site, country) ..."));
    m_filter->setToolTip(tr("With text here the whole database is searched instead of the "
                            "frequencies nearby. Double-click a row to tune the rig to it."));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);

    m_countLabel = new QLabel;

    auto* controls = new QHBoxLayout;
    controls->addWidget(m_followRig);
    controls->addWidget(m_freqEdit);
    controls->addWidget(m_tolerance);
    controls->addWidget(m_onAirOnly);
    controls->addWidget(m_filter, 1);
    controls->addWidget(m_countLabel);

    // --- table --------------------------------------------------------
    m_table = new QTableView;
    m_table->setModel(m_proxy);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(-1, Qt::AscendingOrder);   // keep model order (on air first)
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(
        m_table->fontMetrics().height() + 6);
    m_table->horizontalHeader()->setSectionsMovable(true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setWordWrap(false);
    connect(m_table, &QTableView::doubleClicked, this, &MainWindow::onRowActivated);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableView::customContextMenuRequested, this, &MainWindow::tableContextMenu);

    QHeaderView* h = m_table->horizontalHeader();
    const int em = m_table->fontMetrics().horizontalAdvance(QLatin1Char('M'));
    h->resizeSection(StationModel::ColDelta, em * 6);
    h->resizeSection(StationModel::ColFrequency, em * 7);
    h->resizeSection(StationModel::ColStatus, em * 7);
    h->resizeSection(StationModel::ColMode, em * 5);
    h->resizeSection(StationModel::ColStation, em * 24);
    h->resizeSection(StationModel::ColLanguage, em * 10);
    h->resizeSection(StationModel::ColTime, em * 9);
    h->resizeSection(StationModel::ColDays, em * 6);
    h->resizeSection(StationModel::ColCountry, em * 13);
    h->resizeSection(StationModel::ColSite, em * 16);
    h->resizeSection(StationModel::ColTarget, em * 12);
    h->resizeSection(StationModel::ColLastHeard, em * 6);
    h->resizeSection(StationModel::ColSource, em * 6);
    h->resizeSection(StationModel::ColRemarks, em * 20);
    // Every column stays user-resizable; widths are saved on close and
    // restored at start (see closeEvent / restoreState below).
    h->setSectionResizeMode(QHeaderView::Interactive);

    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    layout->addLayout(header);
    layout->addLayout(controls);
    layout->addWidget(m_table, 1);
    setCentralWidget(central);

    // --- status bar ---------------------------------------------------
    m_rigStatus = new QLabel;
    m_dbStatus = new QLabel;
    statusBar()->addWidget(m_rigStatus, 1);
    statusBar()->addPermanentWidget(m_dbStatus);

    resize(1000, 600);
}

void MainWindow::applySettings()
{
    m_rig->setEndpoint(m_settings.rigHost, quint16(m_settings.rigPort));
    m_rig->setPollInterval(m_settings.pollIntervalMs);
    if (ScheduleSource* src = m_updater->source(QStringLiteral("eibi")))
    {
        src->setBaseUrl(QUrl(m_settings.eibiUrl));
        src->setEnabled(m_settings.eibiEnabled);
    }
    if (ScheduleSource* src = m_updater->source(QStringLiteral("hfcc")))
    {
        src->setBaseUrl(QUrl(m_settings.hfccUrl));
        src->setEnabled(m_settings.hfccEnabled);
    }
    if (ScheduleSource* src = m_updater->source(QStringLiteral("aoki")))
    {
        src->setBaseUrl(QUrl(m_settings.aokiUrl));
        src->setEnabled(m_settings.aokiEnabled);
    }

    const QSignalBlocker b1(m_tolerance), b2(m_onAirOnly), b3(m_followRig), b4(m_onTopAction);
    m_tolerance->setValue(m_settings.toleranceKHz);
    m_onAirOnly->setChecked(m_settings.onAirOnly);
    m_proxy->setOnAirOnly(m_settings.onAirOnly);
    m_followRig->setChecked(m_settings.followRig);
    m_freqEdit->setReadOnly(m_settings.followRig);
    m_onTopAction->setChecked(m_settings.alwaysOnTop);
    onAlwaysOnTopToggled(m_settings.alwaysOnTop);
}

QString MainWindow::formatKHz(double kHz)
{
    // 7125.940 -> "7 125.940"
    QString s = QString::number(kHz, 'f', 3);
    const int dot = s.indexOf(QLatin1Char('.'));
    for (int i = dot - 3; i > 0; i -= 3)
        s.insert(i, QLatin1Char(' '));
    return s;
}

void MainWindow::updateHeader()
{
    if (m_centreKHz > 0.0)
        m_freqLabel->setText(formatKHz(m_centreKHz) + tr(" kHz"));
    else
        m_freqLabel->setText(QStringLiteral("---.--- kHz"));

    if (m_followRig->isChecked())
        m_modeLabel->setText(m_rigConnected ? m_rigMode : tr("no rig"));
    else
        m_modeLabel->setText(tr("manual"));
}

void MainWindow::setCentreKHz(double kHz, bool fromRig)
{
    if (fromRig && !m_followRig->isChecked())
        return;
    m_centreKHz = kHz;
    if (fromRig)
    {
        const QSignalBlocker b(m_freqEdit);
        m_freqEdit->setText(QString::number(kHz, 'f', 3));
    }
    updateHeader();
    refreshLookup();
}

void MainWindow::refreshLookup()
{
    const QString text = m_filter->text().trimmed();

    StationList list;
    if (!text.isEmpty())
        list = m_db->search(text, enabledSources());
    else if (m_centreKHz > 0.0)
        list = m_db->lookup(m_centreKHz, m_tolerance->value(), enabledSources());

    m_model->setEntries(list, m_centreKHz);
    m_lastEvalMinute = QDateTime::currentDateTimeUtc().time().minute();
    updateCountLabel();
}

void MainWindow::onFilterChanged(const QString&)
{
    refreshLookup();
}

void MainWindow::onRowActivated(const QModelIndex& index)
{
    const QModelIndex src = m_proxy->mapToSource(index);
    const double kHz = m_model->data(m_model->index(src.row(), StationModel::ColFrequency),
                                     StationModel::SortRole).toDouble();
    if (kHz <= 0.0)
        return;
    const QString mode = m_model->data(m_model->index(src.row(), StationModel::ColMode),
                                       Qt::DisplayRole).toString();

    m_filter->clear();   // back to the nearby view

    if (m_rig->isConnected())
    {
        // Send it to the radio and let the display follow the rig's answer.
        m_rig->setFrequency(qRound64(kHz * 1000.0));
        static const QStringList rigModes = {QStringLiteral("AM"), QStringLiteral("USB"),
                                             QStringLiteral("LSB"), QStringLiteral("CW")};
        if (rigModes.contains(mode))
            m_rig->setMode(mode);
        if (!m_followRig->isChecked())
            m_followRig->setChecked(true);
        statusBar()->showMessage(tr("Tuning rig to %1 kHz %2").arg(kHz, 0, 'f', 3).arg(mode), 5000);
        return;
    }

    if (m_followRig->isChecked())
        m_followRig->setChecked(false);   // no rig: manual mode
    m_freqEdit->setText(QString::number(kHz, 'f', 3));
    setCentreKHz(kHz, false);
}

void MainWindow::updateCountLabel()
{
    if (m_model->rowCount() == 0)
    {
        m_countLabel->setText(tr("no entries"));
        return;
    }
    m_countLabel->setText((m_filter->text().trimmed().isEmpty()
                               ? tr("%1 on air / %2 near")
                               : tr("%1 on air / %2 found"))
                              .arg(m_model->onAirCount())
                              .arg(m_model->rowCount()));
}

QStringList MainWindow::enabledSources() const
{
    QStringList ids;
    for (ScheduleSource* src : m_updater->sources())
        if (src->isEnabled())
            ids << src->id();
    ids << userSourceId();   // the personal list is always shown
    return ids;
}

void MainWindow::addMyStation()
{
    StationEntry e;
    e.kHz = m_centreKHz;
    e.mode = m_followRig->isChecked() && m_rigConnected ? m_rigMode : QString();
    StationEditDialog dlg(e, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    StationEntry fresh = dlg.entry();
    if (!m_db->insertEntry(fresh))
        QMessageBox::warning(this, tr("My stations"), m_db->lastError());
    refreshLookup();
}

void MainWindow::openMyStations()
{
    MyStationsDialog dlg(m_db, m_centreKHz, m_rigConnected ? m_rigMode : QString(), this);
    dlg.exec();
    refreshLookup();
}

void MainWindow::tableContextMenu(const QPoint& pos)
{
    const QModelIndex idx = m_proxy->mapToSource(m_table->indexAt(pos));
    QMenu menu(this);
    if (idx.isValid())
    {
        const StationEntry e = m_model->entryAt(idx.row());
        menu.addAction(tr("Tune to %1 kHz").arg(e.kHz), this, [this, idx]() {
            onRowActivated(m_proxy->mapFromSource(idx));
        });
        menu.addSeparator();
        const QUrl modeUrl = SigidWiki::modeUrl(e.mode);
        if (!modeUrl.isEmpty())
            menu.addAction(tr("What does %1 sound like? (sigidwiki)").arg(e.mode), this, [modeUrl]() {
                QDesktopServices::openUrl(modeUrl);
            });
        menu.addAction(tr("Search sigidwiki for \"%1\"").arg(e.station), this, [e]() {
            QDesktopServices::openUrl(SigidWiki::searchUrl(e.station));
        });
        menu.addSeparator();
        if (e.source == userSourceId())
        {
            menu.addAction(tr("Edit my entry..."), this, [this, e]() {
                StationEditDialog dlg(e, this);
                if (dlg.exec() == QDialog::Accepted)
                {
                    m_db->updateEntry(dlg.entry());
                    refreshLookup();
                }
            });
            menu.addAction(tr("Delete my entry"), this, [this, e]() {
                if (QMessageBox::question(this, tr("Delete"), tr("Delete \"%1\" on %2 kHz?")
                                                                    .arg(e.station).arg(e.kHz))
                    == QMessageBox::Yes)
                {
                    m_db->removeEntry(e.id);
                    refreshLookup();
                }
            });
        }
        else
        {
            menu.addAction(tr("Copy to my stations..."), this, [this, e]() {
                StationEntry copy = e;
                copy.id = 0;
                copy.source = userSourceId();
                if (copy.langText.isEmpty())
                    copy.langText = m_db->languageName(e.lang).section(QLatin1Char(':'), 0, 0);
                if (copy.siteText.isEmpty())
                    copy.siteText = m_db->siteName(e.itu, e.site);
                StationEditDialog dlg(copy, this);
                if (dlg.exec() == QDialog::Accepted)
                {
                    StationEntry fresh = dlg.entry();
                    m_db->insertEntry(fresh);
                    refreshLookup();
                }
            });
        }
        menu.addSeparator();
    }
    menu.addAction(tr("Add station at %1 kHz...").arg(m_centreKHz, 0, 'f', 3), this, &MainWindow::addMyStation);
    menu.addAction(tr("My stations..."), this, &MainWindow::openMyStations);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MainWindow::updateDbStatus()
{
    QStringList parts;
    QString tip;
    for (ScheduleSource* src : m_updater->sources())
    {
        if (!src->isEnabled())
            continue;
        const int n = src->count();
        if (n == 0)
        {
            parts << tr("%1: no data").arg(src->displayName());
            continue;
        }
        parts << QStringLiteral("%1 %2: %3").arg(src->displayName(), src->season().toUpper()).arg(n);
        const QDateTime updated = src->lastUpdate().toLocalTime();
        tip += tr("%1 %2: %3 entries, checked %4\n")
                   .arg(src->displayName(), src->season().toUpper())
                   .arg(n)
                   .arg(updated.isValid() ? updated.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                          : tr("never"));
    }
    m_dbStatus->setText(parts.isEmpty() ? tr("No sources enabled") : parts.join(QStringLiteral("  |  ")));
    m_dbStatus->setToolTip(tip.trimmed());
}

void MainWindow::onRigFrequency(qint64 hz)
{
    m_rigHz = hz;
    setCentreKHz(hz / 1000.0, true);
}

void MainWindow::onRigMode(const QString& mode, int)
{
    m_rigMode = mode;
    updateHeader();
}

void MainWindow::onRigState(bool connected, const QString& message)
{
    m_rigConnected = connected;
    m_rigStatus->setText(message);
    updateHeader();
}

void MainWindow::onFrequencyEdited()
{
    if (m_followRig->isChecked())
        return;
    bool ok = false;
    const double kHz = m_freqEdit->text().toDouble(&ok);
    if (ok && kHz > 0.0)
        setCentreKHz(kHz, false);
}

void MainWindow::onFollowToggled(bool follow)
{
    m_settings.followRig = follow;
    m_freqEdit->setReadOnly(follow);
    if (follow && m_rigHz > 0)
        setCentreKHz(m_rigHz / 1000.0, true);
    else
        updateHeader();
    if (!follow)
    {
        m_freqEdit->setFocus();
        m_freqEdit->selectAll();
    }
}

void MainWindow::onToleranceChanged(double kHz)
{
    m_settings.toleranceKHz = kHz;
    refreshLookup();
}

void MainWindow::onOnAirOnlyToggled(bool on)
{
    m_settings.onAirOnly = on;
    m_proxy->setOnAirOnly(on);
    updateCountLabel();
}

void MainWindow::onAlwaysOnTopToggled(bool on)
{
    m_settings.alwaysOnTop = on;
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (visible)
        show();
}

void MainWindow::updateDatabases()
{
    if (m_updater->isBusy())
        return;
    m_updateAction->setEnabled(false);
    m_updater->update(m_settings.refreshDays, true);
}

void MainWindow::onUpdateFinished(bool ok, const QString& message)
{
    m_updateAction->setEnabled(true);
    statusBar()->showMessage(message, ok ? 15000 : 0);
    updateDbStatus();
    if (ok)
        refreshLookup();
    else if (m_db->count() == 0)
        QMessageBox::warning(this, tr("Database update failed"),
                             tr("%1\n\nYou can retry from File > Update databases now.").arg(message));
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(m_settings, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const AppSettings updated = dlg.settings(m_settings);
    m_settings.rigHost = updated.rigHost;
    m_settings.rigPort = updated.rigPort;
    m_settings.pollIntervalMs = updated.pollIntervalMs;
    m_settings.toleranceKHz = updated.toleranceKHz;
    m_settings.refreshDays = updated.refreshDays;
    m_settings.eibiUrl = updated.eibiUrl;
    m_settings.hfccUrl = updated.hfccUrl;
    m_settings.aokiUrl = updated.aokiUrl;
    m_settings.eibiEnabled = updated.eibiEnabled;
    m_settings.hfccEnabled = updated.hfccEnabled;
    m_settings.aokiEnabled = updated.aokiEnabled;
    const bool launcherChanged =
        updated.launchRigctld != m_settings.launchRigctld || updated.rigctldPath != m_settings.rigctldPath
        || updated.rigModel != m_settings.rigModel || updated.rigDevice != m_settings.rigDevice
        || updated.rigBaud != m_settings.rigBaud || updated.rigctldExtra != m_settings.rigctldExtra
        || updated.rigPort != m_settings.rigPort;
    m_settings.launchRigctld = updated.launchRigctld;
    m_settings.rigctldPath = updated.rigctldPath;
    m_settings.rigModel = updated.rigModel;
    m_settings.rigDevice = updated.rigDevice;
    m_settings.rigBaud = updated.rigBaud;
    m_settings.rigctldExtra = updated.rigctldExtra;
    m_settings.save(m_db);
    applySettings();
    if (launcherChanged)
        applyLauncher();
    updateDbStatus();
    refreshLookup();
    if (m_updater->anyStale(m_settings.refreshDays))
        updateDatabases();
}

void MainWindow::tick()
{
    const QDateTime utc = QDateTime::currentDateTimeUtc();
    m_clockLabel->setText(utc.toString(QStringLiteral("HH:mm:ss")) + tr(" UTC"));
    if (utc.time().minute() != m_lastEvalMinute)
    {
        m_lastEvalMinute = utc.time().minute();
        m_model->refreshStatus(utc);
        updateCountLabel();
    }
}

void MainWindow::about()
{
    QMessageBox::about(
        this, tr("About On The Dial"),
        tr("<h3>On The Dial %1</h3>"
           "<p>Shows which shortwave stations are scheduled on the frequency your "
           "receiver is tuned to, following the VFO through Hamlib's rigctld.</p>"
           "<p>Schedule data:</p><ul>"
           "<li><a href=\"http://www.eibispace.de/\">EiBi</a> by Eike Bierwirth, free for third-party software</li>"
           "<li><a href=\"http://www.hfcc.org/data/\">HFCC</a> public data files</li>"
           "<li><a href=\"http://www1.s2.starcat.ne.jp/ndxc/\">Aoki / Bi Newsletter</a> by the Nagoya DXers Circle</li>"
           "</ul><p>Thank you to everyone compiling these lists.</p>"
           "<p>Rig control through <a href=\"https://hamlib.github.io/\">Hamlib</a>'s rigctld. "
           "Web page: <a href=\"https://onthedial.oh2gba.eu/\">onthedial.oh2gba.eu</a></p>"
           "<p>Data directory: %2</p>"
           "<p>Licensed under the GNU GPL v3 or later. Built with Qt %3.</p>")
            .arg(QLatin1String(OTD_VERSION), m_dataDir.toHtmlEscaped(),
                 QLatin1String(qVersion())));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_db->setMeta(QStringLiteral("window.geometry"),
                  QString::fromLatin1(saveGeometry().toBase64()));
    m_db->setMeta(QStringLiteral("window.columns"),
                  QString::fromLatin1(m_table->horizontalHeader()->saveState().toBase64()));
    m_settings.save(m_db);
    QMainWindow::closeEvent(event);
}

void MainWindow::screenshotTo(const QString& file, int delayMs)
{
    QTimer::singleShot(delayMs, this, [this, file]() {
        // grab() renders the widget itself; it also works on Wayland where
        // screen grabbing is not available to applications.
        if (!grab().save(file))
            qWarning("Could not save screenshot to %s", qPrintable(file));
        close();
    });
}
