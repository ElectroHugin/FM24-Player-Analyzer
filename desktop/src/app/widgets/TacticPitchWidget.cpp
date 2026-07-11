#include "TacticPitchWidget.h"

#include "../theming/ThemeManager.h"
#include "core/Constants.h"

#include <QPainter>
#include <QPainterPath>

namespace fm {

namespace {

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

TacticPitchWidget::TacticPitchWidget(ThemeManager &theme, QWidget *parent)
    : QWidget(parent)
    , m_theme(theme)
{
    setMinimumSize(340, 340 * 105 / 68);
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    connect(&m_theme, &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void TacticPitchWidget::setTeam(const QHash<QString, XiCell> &team,
                                const QHash<QString, QString> &positions,
                                const QHash<QString, QStringList> &layout,
                                const QHash<QString, QString> &roleNames, bool showApt)
{
    m_team = team;
    m_positions = positions;
    m_layout = layout;
    m_roleNames = roleNames;
    m_showApt = showApt;
    update();
}

void TacticPitchWidget::setSlotColors(const QHash<QString, QColor> &colors)
{
    m_slotColors = colors;
    update();
}

void TacticPitchWidget::setSlotSubtexts(const QHash<QString, QString> &subtexts)
{
    m_slotSubtexts = subtexts;
    update();
}

void TacticPitchWidget::clearData()
{
    m_team.clear();
    m_positions.clear();
    m_layout.clear();
    m_slotColors.clear();
    m_slotSubtexts.clear();
    update();
}

void TacticPitchWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool night = m_theme.mode() != QLatin1String("day");
    const QColor pitchBg = night ? QColor(0x2a, 0x5d, 0x34) : QColor(0xD3, 0xEE, 0xD8);
    const QColor markings = night ? QColor(255, 255, 255, 100) : QColor(0, 0, 0, 50);
    const QColor boxBg = night ? QColor(0, 0, 0, 100) : QColor(255, 255, 255, 180);
    const QColor boxBorder = night ? QColor(0x55, 0x55, 0x55) : QColor(0xB0, 0xB0, 0xB0);
    const QColor textColor = night ? QColor(Qt::white) : QColor(0x31, 0x33, 0x3F);
    const QColor secondaryText = night ? QColor(0xcc, 0xcc, 0xcc) : QColor(0x55, 0x55, 0x55);

    const QRectF pitch = rect().adjusted(1, 1, -1, -1);

    // --- Pitch + markings ---
    QPainterPath pitchPath;
    pitchPath.addRoundedRect(pitch, 10, 10);
    painter.fillPath(pitchPath, pitchBg);
    painter.setPen(QPen(markings, 2));
    painter.drawPath(pitchPath);
    painter.setClipPath(pitchPath);

    const double cx = pitch.center().x();
    painter.setPen(QPen(markings, 1.5));
    painter.drawLine(QPointF(pitch.left(), pitch.center().y()),
                     QPointF(pitch.right(), pitch.center().y()));
    painter.drawEllipse(pitch.center(), pitch.width() * 0.16, pitch.width() * 0.16);
    const double penW = pitch.width() * 0.40, penH = pitch.height() * 0.18;
    painter.drawRect(QRectF(cx - penW / 2, pitch.top(), penW, penH));
    painter.drawRect(QRectF(cx - penW / 2, pitch.bottom() - penH, penW, penH));
    const double goalW = pitch.width() * 0.20, goalH = pitch.height() * 0.07;
    painter.drawRect(QRectF(cx - goalW / 2, pitch.top(), goalW, goalH));
    painter.drawRect(QRectF(cx - goalW / 2, pitch.bottom() - goalH, goalW, goalH));

    if (m_layout.isEmpty() && !m_positions.contains(QStringLiteral("GK"))) {
        painter.setPen(textColor);
        painter.drawText(pitch, Qt::AlignCenter, tr("Keine Daten"));
        return;
    }

    // --- Cell grid: 5 columns x 6 rows, GK spanning the bottom row ---
    const double padding = 10.0, gap = 6.0;
    const double cellW = (pitch.width() - 2 * padding - 4 * gap) / 5.0;
    const double cellH = (pitch.height() - 2 * padding - 5 * gap) / 6.0;
    const auto cellRect = [&](int row, int col) {
        return QRectF(pitch.left() + padding + (col - 1) * (cellW + gap),
                      pitch.top() + padding + (row - 1) * (cellH + gap), cellW, cellH);
    };

    QFont nameFont = font();
    nameFont.setBold(true);
    QFont ratingFont = font();
    ratingFont.setBold(true);
    ratingFont.setPointSizeF(font().pointSizeF() * 1.1);
    QFont smallFont = font();
    smallFont.setPointSizeF(font().pointSizeF() * 0.82);
    smallFont.setItalic(true);

    const auto drawBox = [&](const QRectF &box, const QString &slot) {
        const XiCell cell = m_team.value(slot);
        QPainterPath path;
        path.addRoundedRect(box, 6, 6);
        const QColor background = m_slotColors.value(slot, boxBg);
        painter.fillPath(path, background);
        painter.setPen(QPen(boxBorder, 1));
        painter.drawPath(path);

        const QString roleAbbr = m_positions.value(slot);
        const QString apt = aptAbbreviations().value(cell.apt, cell.apt);
        const QString subtext = m_slotSubtexts.value(slot);

        // Stack: name / rating / (role) / apt-or-subtext.
        const double lineH = box.height() / 4.2;
        QRectF line(box.left() + 2, box.top() + box.height() * 0.06, box.width() - 4, lineH);
        painter.setPen(textColor);
        painter.setFont(nameFont);
        const QString name = cell.isFilled() ? cell.name : QStringLiteral("–");
        painter.drawText(line, Qt::AlignHCenter | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(name, Qt::ElideRight,
                                                          static_cast<int>(line.width())));
        line.translate(0, lineH);
        painter.setFont(ratingFont);
        painter.drawText(line, Qt::AlignHCenter | Qt::AlignVCenter,
                         cell.isFilled() ? QStringLiteral("%1%").arg(qRound(cell.rating))
                                         : QStringLiteral("0%"));
        line.translate(0, lineH);
        painter.setPen(secondaryText);
        painter.setFont(smallFont);
        painter.drawText(line, Qt::AlignHCenter | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(
                             QStringLiteral("(%1)").arg(roleAbbr), Qt::ElideRight,
                             static_cast<int>(line.width())));
        const QString lastLine = !subtext.isEmpty() ? subtext : (m_showApt ? apt : QString());
        if (!lastLine.isEmpty()) {
            line.translate(0, lineH);
            painter.drawText(line, Qt::AlignHCenter | Qt::AlignVCenter,
                             painter.fontMetrics().elidedText(
                                 lastLine, Qt::ElideRight, static_cast<int>(line.width())));
        }
    };

    for (auto it = m_layout.constBegin(); it != m_layout.constEnd(); ++it) {
        const int row = rowForStratum(it.key());
        if (row < 0)
            continue;
        for (const QString &slot : it.value()) {
            if (slot == QLatin1String("GK"))
                continue;
            const auto posIt = masterPositionMap().constFind(slot);
            if (posIt == masterPositionMap().constEnd())
                continue;
            const int col = posIt.value().column
                            + (it.key() == QLatin1String("Strikers") ? 2 : 1);
            if (col < 1 || col > 5)
                continue;
            drawBox(cellRect(row, col), slot);
        }
    }

    if (m_positions.contains(QStringLiteral("GK"))) {
        const QRectF gkRect(pitch.left() + padding,
                            pitch.top() + padding + 5 * (cellH + gap),
                            pitch.width() - 2 * padding, cellH);
        drawBox(gkRect, QStringLiteral("GK"));
    }
}

} // namespace fm
