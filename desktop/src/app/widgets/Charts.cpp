#include "Charts.h"

#include "../theming/ThemeManager.h"

#include <QAreaSeries>
#include <QCategoryAxis>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QGraphicsSimpleTextItem>
#include <QLineSeries>
#include <QPolarChart>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <limits>

namespace fm {

namespace {

// QChartView::setChart does not delete the previous chart.
void swapChart(QChartView *view, QChart *chart)
{
    QChart *old = view->chart();
    view->setChart(chart);
    delete old;
}

} // namespace

QList<QColor> chartTracePalette(const ThemeManager &theme)
{
    if (theme.mode() == QLatin1String("day")) {
        return {theme.primary(), QColor(0xD3, 0x2F, 0x2F), QColor(0x7B, 0x1F, 0xA2),
                QColor(0x02, 0x88, 0xD1), QColor(0xFF, 0xA0, 0x00)};
    }
    return {theme.primary(), QColor(0xF5, 0x00, 0x57), QColor(0x00, 0xE5, 0xFF),
            QColor(0xFF, 0xDE, 0x03), QColor(0x76, 0xFF, 0x03)};
}

namespace {

void applyChartTheme(QChart *chart, const ThemeManager &theme)
{
    chart->setBackgroundBrush(Qt::NoBrush);
    chart->setBackgroundPen(Qt::NoPen);
    chart->setPlotAreaBackgroundBrush(QColor(theme.secondaryBackground()));
    chart->setPlotAreaBackgroundVisible(true);
    chart->legend()->setLabelColor(theme.text());
    chart->setMargins(QMargins(4, 4, 4, 4));
}

void styleAxis(QAbstractAxis *axis, const ThemeManager &theme)
{
    axis->setLabelsColor(theme.text());
    axis->setTitleBrush(theme.text());
    const QColor grid = theme.mode() == QLatin1String("day") ? QColor(0, 0, 0, 50)
                                                             : QColor(255, 255, 255, 60);
    axis->setGridLineColor(grid);
    axis->setLinePenColor(grid);
}

} // namespace

// --- LineChartWidget ---

LineChartWidget::LineChartWidget(ThemeManager &theme, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_view = new QChartView(this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setMinimumHeight(320);
    layout->addWidget(m_view);
    connect(&m_theme, &ThemeManager::themeChanged, this, &LineChartWidget::rebuild);
    clearChart(tr("Keine Daten"));
}

void LineChartWidget::setSeries(QList<Series> series)
{
    m_series = std::move(series);
    m_emptyMessage = tr("Noch keine historischen DWRS-Daten vorhanden.");
    rebuild();
}

void LineChartWidget::clearChart(const QString &message)
{
    m_series.clear();
    m_emptyMessage = message;
    rebuild();
}

void LineChartWidget::rebuild()
{
    auto *chart = new QChart;
    applyChartTheme(chart, m_theme);

    if (m_series.isEmpty()) {
        chart->setTitle(m_emptyMessage);
        chart->setTitleBrush(m_theme.text());
        swapChart(m_view, chart);
        return;
    }

    const QList<QColor> palette = chartTracePalette(m_theme);
    double minY = 100.0, maxY = 0.0;
    qint64 minX = std::numeric_limits<qint64>::max(), maxX = 0;

    int index = 0;
    for (const Series &series : m_series) {
        auto *line = new QLineSeries(chart);
        line->setName(series.name);
        QPen pen(palette.at(index % palette.size()));
        pen.setWidth(2);
        line->setPen(pen);
        for (const QPointF &point : series.points) {
            line->append(point);
            minY = std::min(minY, point.y());
            maxY = std::max(maxY, point.y());
            minX = std::min(minX, static_cast<qint64>(point.x()));
            maxX = std::max(maxX, static_cast<qint64>(point.x()));
        }
        chart->addSeries(line);
        ++index;
    }

    auto *xAxis = new QDateTimeAxis(chart);
    xAxis->setFormat(QStringLiteral("dd.MM.yy"));
    xAxis->setRange(QDateTime::fromMSecsSinceEpoch(minX),
                    QDateTime::fromMSecsSinceEpoch(std::max(maxX, minX + 1)));
    auto *yAxis = new QValueAxis(chart);
    const double padding = std::max(2.0, (maxY - minY) * 0.1);
    yAxis->setRange(std::max(0.0, minY - padding), std::min(100.0, maxY + padding));
    yAxis->setLabelFormat(QStringLiteral("%d%%"));
    styleAxis(xAxis, m_theme);
    styleAxis(yAxis, m_theme);
    chart->addAxis(xAxis, Qt::AlignBottom);
    chart->addAxis(yAxis, Qt::AlignLeft);
    for (QAbstractSeries *series : chart->series()) {
        series->attachAxis(xAxis);
        series->attachAxis(yAxis);
    }

    swapChart(m_view, chart);
}

// --- RadarChartWidget ---

RadarChartWidget::RadarChartWidget(ThemeManager &theme, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_view = new QChartView(this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setMinimumHeight(360);
    layout->addWidget(m_view);
    connect(&m_theme, &ThemeManager::themeChanged, this, &RadarChartWidget::rebuild);
    rebuild();
}

void RadarChartWidget::setData(const QStringList &categories,
                               const QList<QPair<QString, QList<double>>> &traces,
                               double maxValue)
{
    m_categories = categories;
    m_traces = traces;
    m_maxValue = maxValue;
    rebuild();
}

void RadarChartWidget::rebuild()
{
    auto *chart = new QPolarChart;
    applyChartTheme(chart, m_theme);

    const int n = static_cast<int>(m_categories.size());
    if (n == 0 || m_traces.isEmpty()) {
        chart->setTitle(tr("Spieler auswählen, um den Vergleich zu sehen."));
        chart->setTitleBrush(m_theme.text());
        swapChart(m_view, chart);
        return;
    }

    auto *angularAxis = new QCategoryAxis(chart);
    angularAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    angularAxis->setRange(0, 360);
    for (int i = 0; i < n; ++i)
        angularAxis->append(m_categories.at(i), i * 360.0 / n);
    auto *radialAxis = new QValueAxis(chart);
    radialAxis->setRange(0, m_maxValue);
    radialAxis->setLabelFormat(QStringLiteral("%d"));
    styleAxis(angularAxis, m_theme);
    styleAxis(radialAxis, m_theme);
    chart->addAxis(angularAxis, QPolarChart::PolarOrientationAngular);
    chart->addAxis(radialAxis, QPolarChart::PolarOrientationRadial);

    const QList<QColor> palette = chartTracePalette(m_theme);
    int index = 0;
    for (const auto &[name, values] : m_traces) {
        auto *line = new QLineSeries(chart);
        line->setName(name);
        for (int i = 0; i < n && i < values.size(); ++i)
            line->append(i * 360.0 / n, values.at(i));
        if (!values.isEmpty())
            line->append(360.0, values.first()); // close the shape

        const QColor color = palette.at(index % palette.size());
        auto *area = new QAreaSeries(line);
        area->setName(name);
        QColor fill = color;
        fill.setAlphaF(0.2f);
        area->setBrush(fill);
        QPen pen(color);
        pen.setWidth(2);
        area->setPen(pen);

        chart->addSeries(area);
        area->attachAxis(angularAxis);
        area->attachAxis(radialAxis);
        ++index;
    }

    swapChart(m_view, chart);
}

} // namespace fm
