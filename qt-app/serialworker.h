#pragma once
#include <QObject>
#include <QSerialPort>

class SerialWorker : public QObject {
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void open(const QString &portName);
    void close();

signals:
    void lineReceived(const QString &line);
    void status(const QString &message);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError e);

private:
    QSerialPort *m_port = nullptr;
    QByteArray m_buf;
};