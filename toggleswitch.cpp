#include "toggleswitch.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent), m_animation(new QPropertyAnimation(this, "offset", this))
{
    setFixedSize(46, 24);
    setCursor(Qt::PointingHandCursor);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;

    m_animation->setStartValue(m_offset);
    m_animation->setEndValue(m_checked ? width() - height() : 0);
    m_animation->start();

    emit toggled(m_checked);
}

void ToggleSwitch::setOffset(int offset)
{
    m_offset = offset;
    update();
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked);
    }
    QWidget::mouseReleaseEvent(event);
}

void ToggleSwitch::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Фон переключателя
    QColor bgColor = m_checked ? QColor("#00d2ff") : QColor("#555566");
    if (m_hover) bgColor = bgColor.lighter(110);

    QPainterPath bgPath;
    bgPath.addRoundedRect(rect(), height() / 2.0, height() / 2.0);
    painter.fillPath(bgPath, bgColor);

    // Кружок
    int margin = 2;
    int circleSize = height() - margin * 2;
    QRectF circleRect(m_offset + margin, margin, circleSize, circleSize);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#ffffff"));
    painter.drawEllipse(circleRect);
}