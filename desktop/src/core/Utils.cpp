#include "Utils.h"

#include "GistRainbowLut.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace fm {

double valueToFloat(const QString &valueStr)
{
    if (valueStr.isEmpty())
        return 0.0;

    if (valueStr.contains(QLatin1String("not for sale"), Qt::CaseInsensitive))
        return kUnbuyableValue;

    QString s = valueStr;
    // Ranges like "€500K - €800K": legacy takes the lower bound.
    const int rangeSep = s.indexOf(QLatin1String(" - "));
    if (rangeSep >= 0)
        s = s.left(rangeSep);

    s.remove(QChar(0x20AC)); // €
    s = s.trimmed();

    double multiplier = 1.0;
    if (s.contains(QLatin1Char('M'))) {
        multiplier = 1'000'000.0;
        s.remove(QLatin1Char('M'));
    } else if (s.contains(QLatin1Char('K'))) {
        multiplier = 1'000.0;
        s.remove(QLatin1Char('K'));
    }

    if (s.isEmpty())
        return 0.0;

    bool ok = false;
    const double value = s.toDouble(&ok);
    return ok ? value * multiplier : 0.0;
}

QString getLastName(const QString &fullName)
{
    if (fullName.isEmpty())
        return QString();
    const qsizetype lastSpace = fullName.lastIndexOf(QLatin1Char(' '));
    return lastSpace < 0 ? fullName : fullName.mid(lastSpace + 1);
}

QString foldForSearch(const QString &text)
{
    // Accent-/umlaut-insensitive search key: decompose (NFD) so diacritics
    // become separate combining marks, drop those marks, lowercase, and map the
    // few Latin letters that do not decompose. "Müller"/"Muller" -> "muller",
    // "Håland" -> "haland", "Gießen" -> "giessen".
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar ch : decomposed) {
        if (ch.category() == QChar::Mark_NonSpacing)
            continue; // combining accent
        out.append(ch);
    }
    out = out.toLower();
    out.replace(QChar(0x00DF), QStringLiteral("ss")); // ß
    out.replace(QChar(0x00F8), QLatin1Char('o'));     // ø
    out.replace(QChar(0x0142), QLatin1Char('l'));     // ł
    out.replace(QChar(0x0111), QLatin1Char('d'));     // đ
    out.replace(QChar(0x00E6), QStringLiteral("ae")); // æ
    out.replace(QChar(0x0153), QStringLiteral("oe")); // œ
    return out;
}

QSet<QString> parsePositionString(const QString &posStr)
{
    QSet<QString> result;
    if (posStr.isEmpty())
        return result;

    // Same pattern as legacy: bases like "D/WB", optional "(RLC)" side group.
    static const QRegularExpression re(
        QStringLiteral(R"(^([A-Z/]+) *(?:\(([RLC]+)\))?$)"));

    const QStringList parts = posStr.split(QLatin1Char(','));
    for (const QString &rawPart : parts) {
        const QString part = rawPart.trimmed();
        const QRegularExpressionMatch match = re.match(part);
        if (!match.hasMatch())
            continue;
        const QString bases = match.captured(1);
        const QString sides = match.captured(2);
        const QStringList baseList = bases.split(QLatin1Char('/'));
        for (const QString &base : baseList) {
            if (!sides.isEmpty()) {
                for (const QChar side : sides)
                    result.insert(base + QStringLiteral(" (") + side + QLatin1Char(')'));
            } else {
                result.insert(base == QLatin1String("ST") ? QStringLiteral("ST (C)") : base);
            }
        }
    }
    return result;
}

QDateTime parseDwrsTimestamp(const QString &timestamp)
{
    return QDateTime::fromString(timestamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

double relativeLuminance(const QColor &color)
{
    const auto channel = [](int v) {
        const double s = v / 255.0;
        return s <= 0.03928 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return channel(color.red()) * 0.2126
         + channel(color.green()) * 0.7152
         + channel(color.blue()) * 0.0722;
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double lumA = relativeLuminance(a);
    const double lumB = relativeLuminance(b);
    const double lighter = std::max(lumA, lumB);
    const double darker = std::min(lumA, lumB);
    return (lighter + 0.05) / (darker + 0.05);
}

CellStyle dwrsCellStyle(double normalizedValue)
{
    // Same normalization window as legacy: Normalize(vmin=30, vmax=95), clipped.
    constexpr double vmin = 30.0;
    constexpr double vmax = 95.0;
    double t = (normalizedValue - vmin) / (vmax - vmin);
    t = std::clamp(t, 0.0, 1.0);

    // matplotlib maps a float through its 256-entry LUT as int(t * N), N=256.
    const int lutIndex = std::clamp(static_cast<int>(t * 256.0), 0, 255);
    const auto &rgb = kGistRainbowLut[lutIndex];
    const QColor background(rgb[0], rgb[1], rgb[2]);

    CellStyle style;
    style.background = background;
    style.text = relativeLuminance(background) > 0.5 ? QColorConstants::Black
                                                     : QColorConstants::White;
    return style;
}

CellStyle attributeCellStyle(int value)
{
    CellStyle style;
    // Tiers as in legacy: 1-7 red, 8-11 orange, 12-14 yellow, 15-17 light
    // green, 18-20 full green.
    if (value >= 18) {
        style.background = QColor(QStringLiteral("#0da025"));
        style.text = QColorConstants::White;
    } else if (value >= 15) {
        style.background = QColor(QStringLiteral("#a2d31a"));
        style.text = QColorConstants::Black;
    } else if (value >= 12) {
        style.background = QColor(QStringLiteral("#d8d21e"));
        style.text = QColorConstants::Black;
    } else if (value >= 8) {
        style.background = QColor(QStringLiteral("#ca7b3a"));
        style.text = QColorConstants::Black;
    } else if (value >= 1) {
        style.background = QColor(QStringLiteral("#cf1e1e"));
        style.text = QColorConstants::Black;
    }
    return style;
}

CellStyle personalityCellStyle(const QString &category)
{
    CellStyle style;
    if (category == QLatin1String("good")) {
        style.background = QColor(QStringLiteral("#0da025"));
        style.text = QColorConstants::White;
    } else if (category == QLatin1String("neutral")) {
        style.background = QColor(QStringLiteral("#d8d21e"));
        style.text = QColorConstants::Black;
    } else if (category == QLatin1String("bad")) {
        style.background = QColor(QStringLiteral("#cf1e1e"));
        style.text = QColorConstants::White;
    }
    return style;
}

} // namespace fm
