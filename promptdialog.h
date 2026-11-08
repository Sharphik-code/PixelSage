#ifndef PROMPTDIALOG_H
#define PROMPTDIALOG_H

#include <QDialog>

class QPlainTextEdit;

class PromptDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PromptDialog(const QString& currentPrompt,
                          const QString& defaultPrompt,
                          QWidget* parent = nullptr);

    QString prompt() const;

private:
    QPlainTextEdit* m_edit = nullptr;
    QString m_default;
};

#endif