#pragma once

#include "core/SquadBuilder.h"

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QWidget>

namespace fm {

class ThemeManager;

// Full-size tactical pitch with one box per slot: player name, rating, role
// and (optionally) playing time. Port of legacy display_tactic_grid.
// Optional per-slot colors turn it into the gap-analysis heat pitch.
class TacticPitchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TacticPitchWidget(ThemeManager &theme, QWidget *parent = nullptr);

    // team: slot -> cell; positions: slot -> role abbr;
    // layout: stratum -> slots (Definitions::tacticLayouts()[tactic]).
    void setTeam(const QHash<QString, XiCell> &team, const QHash<QString, QString> &positions,
                 const QHash<QString, QStringList> &layout,
                 const QHash<QString, QString> &roleNames, bool showApt = true);

    // Optional background override per slot (gap heat map); empty = default.
    void setSlotColors(const QHash<QString, QColor> &colors);
    // Optional second line under the name (e.g. gap reason), per slot.
    void setSlotSubtexts(const QHash<QString, QString> &subtexts);

    void clearData();

signals:
    // Player interactions (M13): boxes are clickable.
    void playerDoubleClicked(const QString &uid);
    void playerContextMenuRequested(const QString &uid, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return width * 105 / 68; }
    QSize sizeHint() const override { return {480, 480 * 105 / 68}; }

private:
    QString uidAt(const QPoint &pos) const;

    ThemeManager &m_theme;
    QHash<QString, QRectF> m_boxRects; // slot -> painted rect (for hit tests)
    QHash<QString, XiCell> m_team;
    QHash<QString, QString> m_positions;
    QHash<QString, QStringList> m_layout;
    QHash<QString, QString> m_roleNames;
    QHash<QString, QColor> m_slotColors;
    QHash<QString, QString> m_slotSubtexts;
    bool m_showApt = true;
};

} // namespace fm
