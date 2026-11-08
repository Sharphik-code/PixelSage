#include "descriptiondialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QApplication>
#include <QClipboard>
#include <QGridLayout>

DescriptionDialog::DescriptionDialog(const DescriptionManager::Description& desc, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Информация о фото: %1").arg(desc.fileName));
    setMinimumSize(650, 450);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout(this);

    // Сетка с метаданными
    auto* metaLayout = new QGridLayout();
    metaLayout->setSpacing(10);

    QLabel* lblFile = new QLabel(QString("<b>Файл:</b> %1").arg(desc.fileName), this);
    QLabel* lblLang = new QLabel(QString("<b>Язык:</b> %1").arg(desc.lang), this);
    QLabel* lblModel = new QLabel(QString("<b>Модель:</b> %1").arg(desc.model), this);
    QLabel* lblDate = new QLabel(QString("<b>Дата:</b> %1").arg(desc.generatedAt), this);

    lblFile->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lblLang->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lblModel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lblDate->setTextInteractionFlags(Qt::TextSelectableByMouse);

    metaLayout->addWidget(lblFile, 0, 0, 1, 2);
    metaLayout->addWidget(lblLang, 1, 0);
    metaLayout->addWidget(lblModel, 1, 1);
    metaLayout->addWidget(lblDate, 2, 0, 1, 2);

    layout->addLayout(metaLayout);

    auto* label = new QLabel(tr("<b>Сгенерированное описание:</b>"), this);
    label->setStyleSheet("margin-top: 10px;");
    layout->addWidget(label);

    m_text = new QPlainTextEdit(this);
    m_text->setPlainText(desc.description);
    m_text->setReadOnly(true);
    m_text->setStyleSheet(
        "QPlainTextEdit {"
        "  background: #1e1e1e;"
        "  color: #d4d4d4;"
        "  font-family: Consolas, monospace;"
        "  font-size: 13px;"
        "  border: 1px solid #444;"
        "  padding: 8px;"
        "  border-radius: 6px;"
        "}");
    layout->addWidget(m_text, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* btnCopy = new QPushButton(tr("Копировать текст"), this);
    connect(btnCopy, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_text->toPlainText());
    });
    btnRow->addWidget(btnCopy);

    auto* btnRegen = new QPushButton(tr("Перегенерировать"), this);
    connect(btnRegen, &QPushButton::clicked, this, [this]() {
        emit regenerateRequested();
        close();
    });
    btnRow->addWidget(btnRegen);

    auto* btnClose = new QPushButton(tr("Закрыть"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
    btnRow->addWidget(btnClose);

    layout->addLayout(btnRow);
}