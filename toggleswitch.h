#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>
#include <QPropertyAnimation>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int offset READ offset WRITE setOffset)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool checked);
    int offset() const { return m_offset; }
    void setOffset(int offset);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_checked = false;
    int m_offset = 0;
    bool m_hover = false;
    QPropertyAnimation *m_animation;
};

#endif