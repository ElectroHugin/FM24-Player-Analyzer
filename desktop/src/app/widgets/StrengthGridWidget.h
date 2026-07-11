#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QWidget>

namespace fm {

class ThemeManager;

// Miniaturized, color-coded tactical pitch showing the average, min and max
// DWRS per position of the selected tactic. Colors are relative to the
// squad's own strength (red = weakest position, green = strongest).
// Port of legacy ui_components.display_strength_grid.
class StrengthGridWidget : public QWidget
{
    Q_OBJECT

public:
    struct SlotStrength {
        double avg = 0.0;
        double min = 0.0;
        double max = 0.0;
    };

    explicit StrengthGridWidget(ThemeManager &theme, QWidget *parent = nullptr);

    // layout: stratum name -> tactic slots (Definitions::tacticLayouts()[tactic]).
    void setData(const QHash<QString, SlotStrength> &strengths,
                 const QHash<QString, QStringList> &layout);
    void clearData();

    bool hasData() const { return !m_layout.isEmpty(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor relativeColor(double value, double minAvg, double maxAvg) const;

    ThemeManager &m_theme;
    QHash<QString, SlotStrength> m_strengths;
    QHash<QString, QStringList> m_layout;
};

} // namespace fm
