#pragma once
#include <QWidget>
#include <limits>

class GaugeWidget : public QWidget {
    Q_OBJECT
public:
    GaugeWidget(const QString &title, const QString &unit, double min, double max,
                int decimals = 1, QWidget *parent = nullptr);
    void setThresholds(double warn, double crit, bool lowIsBad = false);
    void setValue(double v);
    void setStale(bool s);
    QSize sizeHint() const override { return QSize(220, 220); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QColor colorFor(double v) const;
    QString m_title, m_unit;
    double m_min, m_max, m_value = 0;
    double m_warn = std::numeric_limits<double>::infinity();
    double m_crit = std::numeric_limits<double>::infinity();
    bool m_lowIsBad = false;
    int m_decimals;
    bool m_stale = true;
};