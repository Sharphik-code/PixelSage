#ifndef DESCRIPTIONDIALOG_H
#define DESCRIPTIONDIALOG_H

#include <QDialog>
#include "descriptionmanager.h"

class QPlainTextEdit;

class DescriptionDialog : public QDialog
{
    Q_OBJECT
public:
    // Изменен конструктор: теперь принимает полную структуру описания
    explicit DescriptionDialog(const DescriptionManager::Description& desc, QWidget* parent = nullptr);

signals:
    void regenerateRequested();

private:
    QPlainTextEdit* m_text = nullptr;
};

#endif