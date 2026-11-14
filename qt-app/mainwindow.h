#pragma once
#include <QMainWindow>
#include <QThread>
#include <array>
#include "dashboardmodel.h"

class QComboBox; class QPushButton; class QCheckBox; class QLabel; class QPlainTextEdit;
class SerialWorker; class SimSource; class GaugeWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
signals:
    void requestOpen(const QString &port);
    void requestClose();
private slots:
    void onLine(const QString &line);
    void onConnectClicked();
    void refreshPorts();
private:
    void refreshChannel(Channel c);
    void updateBanner();

    QComboBox *m_ports; QPushButton *m_connect;
    QCheckBox *m_sim, *m_nodeA, *m_nodeB, *m_showLog;
    QLabel *m_banner; QPlainTextEdit *m_log;
    std::array<GaugeWidget*, size_t(Channel::Count)> m_gauges{};   // vide pour les heartbeats
    DashboardModel m_model;
    QThread m_thread;
    SerialWorker *m_worker;
    SimSource *m_simSource;
    bool m_connected = false;
};