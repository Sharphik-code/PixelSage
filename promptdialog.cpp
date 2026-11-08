#include "promptdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

PromptDialog::PromptDialog(const QString& currentPrompt,
                           const QString& defaultPrompt,
                           QWidget* parent)
    : QDialog(parent)
    , m_default(defaultPrompt)
{
    setWindowTitle(tr("Промпт для генерации описаний"));
    setMinimumSize(620, 320);

    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        tr("Промпт передаётся модели для каждой картинки.\n"
           "Английский язык даёт лучшее качество описаний."), this);
    hint->setStyleSheet("color: #888;");
    layout->addWidget(hint);

    m_edit = new QPlainTextEdit(this);
    m_edit->setPlainText(currentPrompt);
    m_edit->setStyleSheet(
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; "
        "font-family: Consolas, monospace; font-size: 12px; "
        "border: 1px solid #444; padding: 6px; }");
    layout->addWidget(m_edit, 1);

    auto* row = new QHBoxLayout();

    auto* btnReset = new QPushButton(tr("Сбросить"), this);
    connect(btnReset, &QPushButton::clicked, this, [this]() {
        m_edit->setPlainText(m_default);
    });
    row->addWidget(btnReset);

    row->addStretch();

    auto* btnCancel = new QPushButton(tr("Отмена"), this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(btnCancel);

    auto* btnOk = new QPushButton(tr("Сохранить"), this);
    btnOk->setDefault(true);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(btnOk);

    layout->addLayout(row);
}

QString PromptDialog::prompt() const
{
    return m_edit->toPlainText().trimmed();
}