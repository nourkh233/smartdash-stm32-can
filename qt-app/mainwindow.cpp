#include "mainwindow.h"
#include "serialworker.h"
#include "simsource.h"
#include "framedecoder.h"
#include "gaugewidget.h"
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStatusBar>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("SmartDash");
    resize(900, 720);
    qApp->setStyleSheet(
        "QWidget { background:#0b1220; color:#e5e7eb; }"
        "QPushButton { background:#1f2937; border:1px solid #374151; border-radius:6px; padding:6px 14px; }"
        "QPushButton:hover { background:#374151; }"
        "QComboBox { background:#1f2937; border:1px solid #374151; border-radius:6px; padding:4px 8px; }"
        "QPlainTextEdit { background:#111827; border:1px solid #1f2937; font-family:Consolas; }");

    m_ports = new QComboBox;
    m_connect = new QPushButton("Connecter");
    auto *refresh = new QPushButton("Actualiser");
    m_sim = new QCheckBox("Simulation (sans carte)");
    m_nodeA = new QCheckBox("Nœud A actif");  m_nodeA->setChecked(true);
    m_nodeB = new QCheckBox("Nœud B actif");  m_nodeB->setChecked(true);
    m_showLog = new QCheckBox("Journal");
    m_banner = new QLabel; m_banner->setAlignment(Qt::AlignCenter);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel("Port :")); top->addWidget(m_ports, 1);
    top->addWidget(refresh); top->addWidget(m_connect);
    auto *opts = new QHBoxLayout;
    opts->addWidget(m_sim); opts->addWidget(m_nodeA); opts->addWidget(m_nodeB);
    opts->addStretch(); opts->addWidget(m_showLog);

    auto *speed = new GaugeWidget("VITESSE", "km/h", 0, 120, 1);        speed->setThresholds(90, 110);
    auto *cur   = new GaugeWidget("COURANT", "A", 0, 2, 2);             cur->setThresholds(1.5, 1.8);
    auto *temp  = new GaugeWidget("TEMPÉRATURE", "°C", 0, 50, 1);       temp->setThresholds(40, 45);

    auto *dist  = new GaugeWidget("DISTANCE", "mm", 0, 2000, 0);        dist->setThresholds(500, 200, true);
    m_gauges[size_t(Channel::Speed)] = speed;       m_gauges[size_t(Channel::Current)]  = cur;
    m_gauges[size_t(Channel::Temperature)] = temp;
    m_gauges[size_t(Channel::Distance)] = dist;

    auto *grid = new QGridLayout; grid->setSpacing(12);
    grid->addWidget(speed, 0, 0); grid->addWidget(cur, 0, 1);
    grid->addWidget(temp, 1, 0);  grid->addWidget(dist, 1, 1);

    m_log = new QPlainTextEdit; m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(300); m_log->setMaximumHeight(160); m_log->setVisible(false);
    connect(m_showLog, &QCheckBox::toggled, m_log, &QWidget::setVisible);

    auto *root = new QVBoxLayout;
    root->addLayout(top); root->addLayout(opts); root->addWidget(m_banner);
    root->addLayout(grid, 1); root->addWidget(m_log);
    auto *central = new QWidget; central->setLayout(root); setCentralWidget(central);

    // --- thread série
    m_worker = new SerialWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &MainWindow::requestOpen,  m_worker, &SerialWorker::open);
    connect(this, &MainWindow::requestClose, m_worker, &SerialWorker::close);
    connect(m_worker, &SerialWorker::lineReceived, this, &MainWindow::onLine);
    connect(m_worker, &SerialWorker::status, this, [this](const QString &m) { statusBar()->showMessage(m); });
    m_thread.start();

    // --- simulateur
    m_simSource = new SimSource(this);
    connect(m_simSource, &SimSource::lineReceived, this, &MainWindow::onLine);
    connect(m_nodeA, &QCheckBox::toggled, m_simSource, &SimSource::setNodeA);
    connect(m_nodeB, &QCheckBox::toggled, m_simSource, &SimSource::setNodeB);

    // --- modèle -> affichage
    connect(&m_model, &DashboardModel::changed,      this, [this](Channel c, double) { refreshChannel(c); });
    connect(&m_model, &DashboardModel::staleChanged, this, [this](Channel c, bool)   { refreshChannel(c); });

    connect(m_connect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(refresh,   &QPushButton::clicked, this, &MainWindow::refreshPorts);
    refreshPorts();
    updateBanner();
}

MainWindow::~MainWindow() { m_thread.quit(); m_thread.wait(); }

void MainWindow::refreshPorts() {
    m_ports->clear();
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts())
        m_ports->addItem(p.portName() + " – " + p.description(), p.portName());
}

void MainWindow::onConnectClicked() {
    if (!m_connected) {
        m_model.reset();
        if (m_sim->isChecked()) m_simSource->start();
        else emit requestOpen(m_ports->currentData().toString());
        m_connected = true; m_connect->setText("Déconnecter");
    } else {
        m_simSource->stop(); emit requestClose();
        m_connected = false; m_connect->setText("Connecter");
    }
    updateBanner();
}

void MainWindow::onLine(const QString &line) {
    m_log->appendPlainText(line);
    const auto f = FrameDecoder::parseLine(line);
    if (!f) return;
    for (const Reading &r : FrameDecoder::decode(*f)) m_model.update(r);
}

void MainWindow::refreshChannel(Channel c) {
    if (c == Channel::HbA || c == Channel::HbB) { updateBanner(); return; }
    if (GaugeWidget *g = m_gauges[size_t(c)]) {
        g->setStale(m_model.isStale(c));
        g->setValue(m_model.value(c));
    }
}

void MainWindow::updateBanner() {
    QString text, color;
    if (!m_connected) {
        text = "DÉCONNECTÉ"; color = "#374151";
    } else {
        const bool aStale = m_model.isStale(Channel::HbA), bStale = m_model.isStale(Channel::HbB);
        const bool aSeen  = m_model.everSeen(Channel::HbA), bSeen = m_model.everSeen(Channel::HbB);
        if (!aStale && !bStale) {
            text = "COMMUNICATION OK : nœuds A et B"; color = "#166534";
        } else if ((aStale && !aSeen) || (bStale && !bSeen)) {
            text = "EN ATTENTE DES NŒUDS…"; color = "#b45309";
        } else {
            QStringList lost; if (aStale) lost << "A"; if (bStale) lost << "B";
            text = "PERTE DE COMMUNICATION : nœud " + lost.join(" et "); color = "#b91c1c";
        }
    }
    m_banner->setText(text);
    m_banner->setStyleSheet(QString("background:%1; color:white; font-weight:bold; padding:8px; border-radius:6px;").arg(color));
}