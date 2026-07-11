#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

class QMenu;
class QPushButton;
class QToolButton;

namespace fm {

class Definitions;

// Shared personality filter (port of legacy personality_filter_controls):
// category quick-toggles (good/neutral/bad) plus an optional set of specific
// personalities. Specific selection wins over categories; nothing selected
// means "no filtering".
class PersonalityFilterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PersonalityFilterWidget(const Definitions &definitions, QWidget *parent = nullptr);

    // Personalities present in the current data (fills the specific menu).
    void setAvailablePersonalities(const QStringList &personalities);

    // True if the player's personality passes the current filter.
    bool allows(const QString &personality) const;
    bool isActive() const;

signals:
    void filterChanged();

private:
    void rebuildSpecificMenu();
    QSet<QString> checkedCategories() const;

    const Definitions &m_definitions;
    QPushButton *m_goodButton = nullptr;
    QPushButton *m_neutralButton = nullptr;
    QPushButton *m_badButton = nullptr;
    QToolButton *m_specificButton = nullptr;
    QMenu *m_specificMenu = nullptr;
    QStringList m_available;
    QSet<QString> m_specificChecked;
};

} // namespace fm
