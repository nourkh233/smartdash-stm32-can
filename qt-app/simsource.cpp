#include "simsource.h"
#include <QTimer>
#include <functional>

static QByteArray le16(quint16 v) {
    QByteArray b; b.append(char(v & 0xFF)); b.append(char(v >> 8)); return b;
}
static double tri(qint64 t, qint64 period, double lo, double hi) {   // onde triangulaire
    double x = double(t % period) / double(period);
    x = (x < 0.5) ? x * 2.0 : (1.0 - x) * 2.0;
    return lo + (hi - lo) * x;
}

void SimSource::emitFrame(quint16 id, const QByteArray &d) {
    const bool fromA = (id == 0x130 || id == 0x140 || id == 0x7F0);   // trames du nœud A
    if ((fromA && !m_aOn) || (!fromA && !m_bOn)) return;
    emit lineReceived(QString("%1#%2").arg(id, 3, 16, QChar('0'))
                          .arg(QString::fromLatin1(d.toHex())).toUpper());
}

SimSource::SimSource(QObject *parent) : QObject(parent) {
    auto add = [this](int ms, std::function<void()> fn) {
        auto *t = new QTimer(this);
        t->setInterval(ms);
        connect(t, &QTimer::timeout, this, fn);
        m_timers.append(t);
    };
    add(50,  [this] { emitFrame(0x110, le16(quint16(tri(m_t.elapsed(), 20000, 0, 120) * 10 + 0.5))); });
    add(100, [this] { emitFrame(0x120, le16(quint16(tri(m_t.elapsed(), 15000, 50, 2000) + 0.5))); });
    add(50,  [this] {
        static const double step[3] = { 0.0, 0.8, 1.6 };
        emitFrame(0x130, le16(quint16(qint16(step[(m_t.elapsed() / 5000) % 3] * 100))));
    });
    add(1000, [this] {
        const qint64 t = m_t.elapsed();
        QByteArray d = le16(quint16(qint16(tri(t, 60000, 22, 30) * 10)));
        d += le16(quint16(tri(t, 40000, 40, 60) * 10));
        emitFrame(0x140, d);
    });
    add(500, [this] {
        emitFrame(0x7F0, QByteArray::fromHex(QByteArray::number(m_hbA++, 16).rightJustified(2, '0') + "00"));
        emitFrame(0x7F1, QByteArray::fromHex(QByteArray::number(m_hbB++, 16).rightJustified(2, '0') + "00"));
    });
}

void SimSource::start() { m_t.start(); for (auto *t : m_timers) t->start(); }
void SimSource::stop()  { for (auto *t : m_timers) t->stop(); }