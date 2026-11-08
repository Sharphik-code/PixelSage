#ifndef BATCHREPORTDIALOG_H
#define BATCHREPORTDIALOG_H

#include <QDialog>
#include <QStringList>

class BatchReportDialog : public QDialog
{
    Q_OBJECT
public:
    struct Report {
        int total = 0;         // сколько всего было в очереди
        int success = 0;       // успешно обработано
        int errors = 0;        // с ошибкой
        int skipped = 0;       // пропущено (уже описаны)
        bool cancelled = false;
        QStringList errorFiles;
        qint64 elapsedMs = 0;
    };

    explicit BatchReportDialog(const Report& report, QWidget* parent = nullptr);
};

#endif