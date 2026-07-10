#pragma once

#include <QDialog>

class QLineEdit;

namespace fm {

// First-launch dialog: choose the data directory (databases, config,
// definitions, backups). Preselects %LOCALAPPDATA%\FM24PlayerAnalyzer.
class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FirstRunDialog(const QString &defaultDir, QWidget *parent = nullptr);

    QString chosenDataDir() const;

private:
    QLineEdit *m_pathEdit;
};

} // namespace fm
