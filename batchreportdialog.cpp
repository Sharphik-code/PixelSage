#include "batchreportdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

static QString formatDuration(qint64 ms)
{
    const qint64 sec = ms / 1000;
    if (sec < 60) return QString("%1 сек").arg(sec);
    const qint64 min = sec / 60;
    const qint64 rem = sec % 60;
    return QString("%1 мин %2 сек").arg(min).arg(rem);
}

BatchReportDialog::BatchReportDialog(const Report& r, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Отчёт о генерации описаний"));
    setMinimumSize(560, 380);

    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(
        r.cancelled
            ? tr("<b>Генерация прервана пользователем</b>")
            : tr("<b>Генерация завершена</b>"), this);
    layout->addWidget(title);

    QString text;
    text += tr("Всего в очереди: %1\n").arg(r.total);
    text += tr("Успешно: %1\n").arg(r.success);
    text += tr("С ошибкой: %1\n").arg(r.errors);
    if (r.skipped > 0)
        text += tr("Пропущено (уже были): %1\n").arg(r.skipped);
    text += tr("Затрачено времени: %1\n").arg(formatDuration(r.elapsedMs));

    auto* summary = new QLabel(text, this);
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(summary);

    if (!r.errorFiles.isEmpty()) {
        auto* lbl = new QLabel(tr("Файлы с ошибками:"), this);
        layout->addWidget(lbl);

        auto* list = new QPlainTextEdit(this);
        list->setReadOnly(true);
        list->setPlainText(r.errorFiles.join('\n'));
        list->setStyleSheet(
            "QPlainTextEdit { background: #2a1a1a; color: #e5b0b0; "
            "font-family: Consolas, monospace; font-size: 11px; "
            "border: 1px solid #553333; padding: 4px; }");
        layout->addWidget(list, 1);
    } else {
        layout->addStretch();
    }

    auto* row = new QHBoxLayout();
    row->addStretch();

    auto* btnClose = new QPushButton(tr("Закрыть"), this);
    btnClose->setDefault(true);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(btnClose);

    layout->addLayout(row);
}