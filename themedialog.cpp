#include "themedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>

ThemeDialog::ThemeDialog(const QString& currentTheme, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Тема оформления"));
    setMinimumSize(420, 220);

    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        tr("Выберите тему приложения.\n"
           "«Как в системе» — приложение следует за настройками Windows."), this);
    hint->setStyleSheet("color: #888;");
    layout->addWidget(hint);

    layout->addSpacing(8);

    auto* group = new QButtonGroup(this);

    m_system = new QRadioButton(tr("Как в системе (по умолчанию)"), this);
    m_light  = new QRadioButton(tr("Светлая"), this);
    m_dark   = new QRadioButton(tr("Тёмная"), this);

    group->addButton(m_system, 0);
    group->addButton(m_light, 1);
    group->addButton(m_dark, 2);

    layout->addWidget(m_system);
    layout->addWidget(m_light);
    layout->addWidget(m_dark);

    const QString t = currentTheme.trimmed().toLower();
    if (t == "light")      m_light->setChecked(true);
    else if (t == "dark")  m_dark->setChecked(true);
    else                   m_system->setChecked(true);

    layout->addStretch();

    auto* row = new QHBoxLayout();
    row->addStretch();

    auto* btnCancel = new QPushButton(tr("Отмена"), this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(btnCancel);

    auto* btnOk = new QPushButton(tr("Применить"), this);
    btnOk->setDefault(true);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(btnOk);

    layout->addLayout(row);
}

QString ThemeDialog::selectedTheme() const
{
    if (m_light->isChecked()) return "light";
    if (m_dark->isChecked())  return "dark";
    return "system";
}