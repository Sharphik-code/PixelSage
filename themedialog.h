#ifndef THEMEDIALOG_H
#define THEMEDIALOG_H

#include <QDialog>
#include <QString>

class QRadioButton;

class ThemeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ThemeDialog(const QString& currentTheme, QWidget* parent = nullptr);

    // Возвращает "system", "light" или "dark"
    QString selectedTheme() const;

private:
    QRadioButton* m_system = nullptr;
    QRadioButton* m_light  = nullptr;
    QRadioButton* m_dark   = nullptr;
};

#endif