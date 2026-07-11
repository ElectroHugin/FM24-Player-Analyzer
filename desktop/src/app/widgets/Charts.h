#pragma once

#include <QList>
#include <QPair>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QWidget>

class QChartView;

namespace fm {

class ThemeManager;

// Theme-aware trace palette (legacy player_comparison palettes).
QList<QColor> chartTracePalette(const ThemeManager &theme);

// Time-series line chart (DWRS development). X values are msecs since epoch.
class LineChartWidget : public QWidget
{
    Q_OBJECT

public:
    struct Series {
        QString name;
        QList<QPointF> points; // x = msecs since epoch, y = normalized DWRS
    };

    explicit LineChartWidget(ThemeManager &theme, QWidget *parent = nullptr);

    void setSeries(QList<Series> series);
    void clearChart(const QString &message);

private:
    void rebuild();

    ThemeManager &m_theme;
    QChartView *m_view = nullptr;
    QList<Series> m_series;
    QString m_emptyMessage;
};

// Radar (polar) chart comparing players over attribute categories (0-20).
class RadarChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RadarChartWidget(ThemeManager &theme, QWidget *parent = nullptr);

    // categories: axis labels; traces: (player name, one value per category).
    void setData(const QStringList &categories,
                 const QList<QPair<QString, QList<double>>> &traces, double maxValue = 20.0);

private:
    void rebuild();

    ThemeManager &m_theme;
    QChartView *m_view = nullptr;
    QStringList m_categories;
    QList<QPair<QString, QList<double>>> m_traces;
    double m_maxValue = 20.0;
};

} // namespace fm
