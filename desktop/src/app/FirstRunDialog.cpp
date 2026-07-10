#include "FirstRunDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace fm {

FirstRunDialog::FirstRunDialog(const QString &defaultDir, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("FM24 Player Analyzer — Ersteinrichtung"));
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("Willkommen! Wähle den Ordner, in dem der Analyzer seine Daten ablegt\n"
           "(Datenbanken, Einstellungen, Rollen-Definitionen, Backups).\n"
           "Du kannst den Ordner später in den Einstellungen ändern."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *row = new QHBoxLayout;
    m_pathEdit = new QLineEdit(QDir::toNativeSeparators(defaultDir), this);
    auto *browse = new QPushButton(tr("Durchsuchen…"), this);
    row->addWidget(m_pathEdit, 1);
    row->addWidget(browse);
    layout->addLayout(row);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Datenordner wählen"), m_pathEdit->text());
        if (!dir.isEmpty())
            m_pathEdit->setText(QDir::toNativeSeparators(dir));
    });

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Loslegen"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Beenden"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString FirstRunDialog::chosenDataDir() const
{
    return QDir::fromNativeSeparators(m_pathEdit->text().trimmed());
}

} // namespace fm
