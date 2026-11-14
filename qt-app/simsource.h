
#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QVector>
#include <QString>
#include <QByteArray>
#include <QtGlobal>

class QTimer;

class SimSource : public QObject
{
    Q_OBJECT

public:
    explicit SimSource(QObject *parent = nullptr);

    void start();
    void stop();

    void setNodeA(bool on) { m_aOn = on; }
    void setNodeB(bool on) { m_bOn = on; }

signals:
    void lineReceived(const QString &line);

private:
    void emitFrame(quint16 id, const QByteArray &data);

    QElapsedTimer m_t;
    QVector<QTimer*> m_timers;

    quint8 m_hbA = 0;
    quint8 m_hbB = 0;

    bool m_aOn = true;
    bool m_bOn = true;
};
