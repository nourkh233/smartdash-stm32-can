#include "dashboardmodel.h"
#include <QTimer>
DashboardModel::DashboardModel(QObject *parent) : QObject(parent) {
    m_clock.start();
    auto *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &DashboardModel::checkStale);
    t->start(200);                                   // vérification toutes les 200 ms
}

int DashboardModel::timeoutMs(Channel c) {
    switch (c) {
    case Channel::Speed: case Channel::Current:   return 500;
    case Channel::Distance:                        return 1000;
    case Channel::Temperature: case Channel::Humidity: return 3000;
    default:                                       return 1500;   // heartbeats
    }
}

void DashboardModel::update(const Reading &r) {
    Slot &s = m_slots[size_t(r.ch)];
    s.value = r.value;
    s.lastMs = m_clock.elapsed();
    if (s.stale) { s.stale = false; emit staleChanged(r.ch, false); }
    emit changed(r.ch, r.value);
}

void DashboardModel::checkStale() {
    const qint64 now = m_clock.elapsed();
    for (int i = 0; i < int(Channel::Count); ++i) {
        Slot &s = m_slots[size_t(i)];
        const bool st = s.lastMs < 0 || (now - s.lastMs) > timeoutMs(Channel(i));
        if (st != s.stale) { s.stale = st; emit staleChanged(Channel(i), st); }
    }
}
void DashboardModel::reset() {
    for (int i = 0; i < int(Channel::Count); ++i) {
        m_slots[size_t(i)] = Slot();
        emit staleChanged(Channel(i), true);
    }
}