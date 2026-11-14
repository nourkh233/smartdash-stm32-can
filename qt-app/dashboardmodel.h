#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <array>
#include "framedecoder.h"
class DashboardModel : public QObject {
    Q_OBJECT
public:
    explicit DashboardModel(QObject *parent = nullptr);
    void update(const Reading &r);
    double value(Channel c) const { return m_slots[size_t(c)].value; }
    bool isStale(Channel c) const { return m_slots[size_t(c)].stale; }
    bool everSeen(Channel c) const { return m_slots[size_t(c)].lastMs >= 0; }
    void reset();

signals:
    void changed(Channel c, double value);
    void staleChanged(Channel c, bool stale);

private slots:
    void checkStale();

private:
    struct Slot { double value = 0; qint64 lastMs = -1; bool stale = true; };
    static int timeoutMs(Channel c);
    std::array<Slot, size_t(Channel::Count)> m_slots;
    QElapsedTimer m_clock;
};