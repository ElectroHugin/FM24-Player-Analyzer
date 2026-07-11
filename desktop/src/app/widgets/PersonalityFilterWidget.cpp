#include "PersonalityFilterWidget.h"

#include "core/Definitions.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>

namespace fm {

PersonalityFilterWidget::PersonalityFilterWidget(const Definitions &definitions, QWidget *parent)
    : QWidget(parent)
    , m_definitions(definitions)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    layout->addWidget(new QLabel(tr("Persönlichkeit:"), this));

    const auto makeToggle = [this, layout](const QString &text) {
        auto *button = new QPushButton(text, this);
        button->setCheckable(true);
        layout->addWidget(button);
        connect(button, &QPushButton::toggled, this, &PersonalityFilterWidget::filterChanged);
        return button;
    };
    m_goodButton = makeToggle(tr("🟢 Gut"));
    m_neutralButton = makeToggle(tr("🟡 Neutral"));
    m_badButton = makeToggle(tr("🔴 Schlecht"));

    m_specificButton = new QToolButton(this);
    m_specificButton->setText(tr("Bestimmte…"));
    m_specificButton->setPopupMode(QToolButton::InstantPopup);
    m_specificMenu = new QMenu(this);
    m_specificButton->setMenu(m_specificMenu);
    layout->addWidget(m_specificButton);
    layout->addStretch(1);
}

void PersonalityFilterWidget::setAvailablePersonalities(const QStringList &personalities)
{
    if (personalities == m_available)
        return;
    m_available = personalities;
    // Drop checked entries that vanished from the data.
    QSet<QString> stillPresent;
    for (const QString &p : personalities) {
        if (m_specificChecked.contains(p))
            stillPresent.insert(p);
    }
    m_specificChecked = stillPresent;
    rebuildSpecificMenu();
}

void PersonalityFilterWidget::rebuildSpecificMenu()
{
    m_specificMenu->clear();
    for (const QString &personality : m_available) {
        QAction *action = m_specificMenu->addAction(personality);
        action->setCheckable(true);
        action->setChecked(m_specificChecked.contains(personality));
        connect(action, &QAction::toggled, this, [this, personality](bool checked) {
            if (checked)
                m_specificChecked.insert(personality);
            else
                m_specificChecked.remove(personality);
            m_specificButton->setText(m_specificChecked.isEmpty()
                                          ? tr("Bestimmte…")
                                          : tr("Bestimmte (%1)").arg(m_specificChecked.size()));
            emit filterChanged();
        });
    }
}

QSet<QString> PersonalityFilterWidget::checkedCategories() const
{
    QSet<QString> categories;
    if (m_goodButton->isChecked())
        categories.insert(QStringLiteral("good"));
    if (m_neutralButton->isChecked())
        categories.insert(QStringLiteral("neutral"));
    if (m_badButton->isChecked())
        categories.insert(QStringLiteral("bad"));
    return categories;
}

bool PersonalityFilterWidget::isActive() const
{
    return !m_specificChecked.isEmpty() || !checkedCategories().isEmpty();
}

bool PersonalityFilterWidget::allows(const QString &personality) const
{
    // Specific selection wins; otherwise category selection; otherwise no-op.
    if (!m_specificChecked.isEmpty())
        return m_specificChecked.contains(personality);
    const QSet<QString> categories = checkedCategories();
    if (categories.isEmpty())
        return true;
    return categories.contains(m_definitions.personalityCategory(personality));
}

} // namespace fm
