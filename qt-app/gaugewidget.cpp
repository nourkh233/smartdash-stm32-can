#include "gaugewidget.h"
#include <QPainter>

GaugeWidget::GaugeWidget(const QString &title, const QString &unit, double min, double max,
                         int decimals, QWidget *parent)
    : QWidget(parent), m_title(title), m_unit(unit), m_min(min), m_max(max), m_decimals(decimals) {
    setMinimumSize(160, 160);
}

void GaugeWidget::setThresholds(double warn, double crit, bool lowIsBad) {
    m_warn = warn; m_crit = crit; m_lowIsBad = lowIsBad; update();
}
void GaugeWidget::setValue(double v) { m_value = v; update(); }
void GaugeWidget::setStale(bool s)   { m_stale = s; update(); }

QColor GaugeWidget::colorFor(double v) const {
    if (m_stale) return QColor(107, 114, 128);                       // gris : donnée périmée
    const bool crit = m_lowIsBad ? v <= m_crit : v >= m_crit;
    const bool warn = m_lowIsBad ? v <= m_warn : v >= m_warn;
    if (crit) return QColor(239, 68, 68);                             // rouge
    if (warn) return QColor(245, 158, 11);                            // orange
    return QColor(34, 197, 94);                                       // vert
}

void GaugeWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double side = qMin(width(), height());
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(side / 220.0, side / 220.0);                 // repère logique 220 x 220, centré

    const QRectF arc(-80, -80, 160, 160);
    const int start = 225 * 16, span = -270 * 16;        // 270° dans le sens horaire

    p.setPen(QPen(QColor(31, 41, 55), 14, Qt::SolidLine, Qt::RoundCap));   // piste
    p.drawArc(arc, start, span);

    if (!m_stale) {
        const double frac = qBound(0.0, (m_value - m_min) / (m_max - m_min), 1.0);
        p.setPen(QPen(colorFor(m_value), 14, Qt::SolidLine, Qt::RoundCap)); // valeur
        if (frac > 0.0) p.drawArc(arc, start, int(span * frac));
    }

    QFont f = font();
    f.setPixelSize(38); f.setBold(true); p.setFont(f);
    p.setPen(m_stale ? QColor(107, 114, 128) : QColor(243, 244, 246));
    p.drawText(QRectF(-70, -38, 140, 60), Qt::AlignCenter,
               m_stale ? QStringLiteral("--") : QString::number(m_value, 'f', m_decimals));

    f.setPixelSize(16); f.setBold(false); p.setFont(f);
    p.setPen(QColor(156, 163, 175));
    p.drawText(QRectF(-70, 18, 140, 22), Qt::AlignCenter, m_unit);

    f.setPixelSize(14); f.setBold(true); p.setFont(f);
    p.drawText(QRectF(-100, 84, 200, 22), Qt::AlignCenter, m_title);

    f.setPixelSize(11); f.setBold(false); p.setFont(f);                    // bornes de l'échelle
    p.drawText(QRectF(-100, 60, 60, 16), Qt::AlignCenter, QString::number(m_min, 'f', 0));
    p.drawText(QRectF(40, 60, 60, 16), Qt::AlignCenter, QString::number(m_max, 'f', 0));
}