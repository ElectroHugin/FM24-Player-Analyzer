#include "StrengthGridWidget.h"

#include "../theming/ThemeManager.h"
#include "core/Constants.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace fm {

namespace {

// Vertical pitch row (1 = top) per stratum; GK is drawn as a spanning row 6.
int rowForStratum(const QString &stratum)
{
    static const QHash<QString, int> rows = {
        {QStringLiteral("Strikers"), 1},
        {QStringLiteral("Attacking Midfield"), 2},
        {QStringLiteral("Midfield"), 3},
        {QStringLiteral("Defensive Midfield"), 4},
        {QStringLiteral("Defense"), 5},
    };
    return rows.value(stratum, -1);
}

} // namespace

StrengthGridWidget::StrengthGridWidget(ThemeManager &theme, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
{
    // Legacy mini-pitch: max-width 280px at a 68:105 pitch aspect ratio.
    setFixedSize(280, 280 * 105 / 68);
    connect(&m_theme, &ThemeManager::themeChanged, this,
            qOverload<>(&QWidget::update));
}

void StrengthGridWidget::setData(const QHash<QString, SlotStrength> &strengths,
                                 const QHash<QString, QStringList> &layout)
{
    m_strengths = strengths;
    m_layout = layout;
    update();
}

void StrengthGridWidget::clearData()
{
    m_strengths.clear();
    m_layout.clear();
    update();
}

QColor StrengthGridWidget::relativeColor(double value, double minAvg, double maxAvg) const
{
    // Legacy red -> yellow -> green gradient, relative to the squad's own
    // spread of positional averages.
    const double delta = maxAvg - minAvg;
    if (delta <= 0.0)
        return QColor(100, 166, 100);

    const double normalized = std::clamp((value - minAvg) / delta, 0.0, 1.0);
    int red, green, blue;
    if (normalized < 0.5) {
        red = 221;
        green = 43 + static_cast<int>((240 - 43) * (normalized * 2));
        blue = 43;
    } else {
        red = 240 - static_cast<int>((240 - 98) * ((normalized - 0.5) * 2));
        green = 240 - static_cast<int>((240 - 186) * ((normalized - 0.5) * 2));
        blue = 43 + static_cast<int>((98 - 43) * ((normalized - 0.5) * 2));
    }
    return QColor(red, green, blue);
}

void StrengthGridWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool night = m_theme.mode() != QLatin1String("day");
    const QColor pitchBg = night ? QColor(0x2a, 0x5d, 0x34) : QColor(0xD3, 0xEE, 0xD8);
    const QColor markings = night ? QColor(255, 255, 255, 76) : QColor(0, 0, 0, 51);
    const QColor textColor = night ? QColor(Qt::white) : QColor(0x31, 0x33, 0x3F);

    const QRectF pitch = rect().adjusted(1, 1, -1, -1);

    // --- Pitch background + markings ---
    QPainterPath pitchPath;
    pitchPath.addRoundedRect(pitch, 8, 8);
    painter.fillPath(pitchPath, pitchBg);
    painter.setPen(QPen(markings, 1));
    painter.drawPath(pitchPath);

    painter.setClipPath(pitchPath);
    const double cx = pitch.center().x();
    painter.drawLine(QPointF(pitch.left(), pitch.center().y()),
                     QPointF(pitch.right(), pitch.center().y()));
    painter.drawEllipse(pitch.center(), 30.0, 30.0);
    const double penW = pitch.width() * 0.40, penH = pitch.height() * 0.16;
    painter.drawRect(QRectF(cx - penW / 2, pitch.top(), penW, penH));
    painter.drawRect(QRectF(cx - penW / 2, pitch.bottom() - penH, penW, penH));
    const double goalW = pitch.width() * 0.20, goalH = pitch.height() * 0.07;
    painter.drawRect(QRectF(cx - goalW / 2, pitch.top(), goalW, goalH));
    painter.drawRect(QRectF(cx - goalW / 2, pitch.bottom() - goalH, goalW, goalH));

    if (m_layout.isEmpty()) {
        painter.setPen(textColor);
        painter.drawText(pitch, Qt::AlignCenter, tr("Keine Daten"));
        return;
    }

    // --- 5x6 cell grid (row 6 = GK, spanning) ---
    const double padding = 8.0, gap = 4.0;
    const double cellW = (pitch.width() - 2 * padding - 4 * gap) / 5.0;
    const double cellH = (pitch.height() - 2 * padding - 5 * gap) / 6.0;
    const auto cellRect = [&](int row, int col) {
        return QRectF(pitch.left() + padding + (col - 1) * (cellW + gap),
                      pitch.top() + padding + (row - 1) * (cellH + gap), cellW, cellH);
    };

    // Relative color range over the occupied positions.
    double minAvg = 0.0, maxAvg = 0.0;
    bool haveAvg = false;
    for (auto it = m_strengths.constBegin(); it != m_strengths.constEnd(); ++it) {
        if (it.value().avg > 0.0) {
            if (!haveAvg) {
                minAvg = maxAvg = it.value().avg;
                haveAvg = true;
            } else {
                minAvg = std::min(minAvg, it.value().avg);
                maxAvg = std::max(maxAvg, it.value().avg);
            }
        }
    }

    QFont avgFont = font();
    avgFont.setBold(true);
    avgFont.setPointSizeF(font().pointSizeF() * 1.15);
    QFont rangeFont = font();
    rangeFont.setPointSizeF(font().pointSizeF() * 0.8);

    const auto drawBox = [&](const QRectF &box, const QString &slot) {
        const SlotStrength stats = m_strengths.value(slot);
        QPainterPath path;
        path.addRoundedRect(box, 4, 4);
        if (stats.avg > 0.0) {
            QColor fill = relativeColor(stats.avg, minAvg, maxAvg);
            fill.setAlphaF(0.85f);
            painter.fillPath(path, fill);
            painter.setPen(QPen(QColor(0, 0, 0, 50), 1));
            painter.drawPath(path);
            painter.setPen(textColor);
            painter.setFont(avgFont);
            QRectF top = box.adjusted(0, box.height() * 0.12, 0, -box.height() * 0.42);
            painter.drawText(top, Qt::AlignHCenter | Qt::AlignVCenter,
                             QStringLiteral("%1%").arg(qRound(stats.avg)));
            painter.setFont(rangeFont);
            QRectF bottom = box.adjusted(0, box.height() * 0.52, 0, -box.height() * 0.08);
            painter.drawText(bottom, Qt::AlignHCenter | Qt::AlignVCenter,
                             QStringLiteral("(%1–%2)")
                                 .arg(qRound(stats.min))
                                 .arg(qRound(stats.max)));
        } else {
            painter.setPen(QPen(markings, 1));
            painter.drawPath(path);
            painter.setPen(textColor);
            painter.setFont(rangeFont);
            painter.drawText(box, Qt::AlignCenter, QStringLiteral("–"));
        }
    };

    for (auto it = m_layout.constBegin(); it != m_layout.constEnd(); ++it) {
        const int row = rowForStratum(it.key());
        for (const QString &slot : it.value()) {
            if (slot == QLatin1String("GK"))
                continue;
            const auto posIt = masterPositionMap().constFind(slot);
            if (posIt == masterPositionMap().constEnd() || row < 0)
                continue;
            // Strikers are shifted one column to the right (legacy grid).
            const int col = posIt.value().column
                            + (it.key() == QLatin1String("Strikers") ? 2 : 1);
            if (col < 1 || col > 5)
                continue;
            drawBox(cellRect(row, col), slot);
        }
    }

    // GK spans the full bottom row.
    const QRectF gkRect(pitch.left() + padding, pitch.top() + padding + 5 * (cellH + gap),
                        pitch.width() - 2 * padding, cellH);
    drawBox(gkRect, QStringLiteral("GK"));
}

} // namespace fm
