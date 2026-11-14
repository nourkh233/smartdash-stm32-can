#include "serialworker.h"

void SerialWorker::open(const QString &portName) {
    close();
    m_port = new QSerialPort(this);                 // créé ici, donc dans le thread du worker
    m_port->setPortName(portName);
    m_port->setBaudRate(115200);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    connect(m_port, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialWorker::onError);

    if (m_port->open(QIODevice::ReadOnly)) emit status("Connecté à " + portName);
    else emit status("Échec d'ouverture de " + portName + " : " + m_port->errorString());
}

void SerialWorker::close() {
    if (m_port) { m_port->close(); m_port->deleteLater(); m_port = nullptr; }
    m_buf.clear();
}

void SerialWorker::onReadyRead() {
    m_buf += m_port->readAll();
    int i;
    while ((i = m_buf.indexOf('\n')) >= 0) {        // une ligne complète est prête
        const QByteArray line = m_buf.left(i).trimmed();
        m_buf.remove(0, i + 1);
        if (!line.isEmpty()) emit lineReceived(QString::fromLatin1(line));
    }
    if (m_buf.size() > 1024) m_buf.clear();         // garde-fou : jamais de tampon infini
}

void SerialWorker::onError(QSerialPort::SerialPortError e) {
    if (e == QSerialPort::NoError) return;
    emit status("Erreur série : " + m_port->errorString());
}