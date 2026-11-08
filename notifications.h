#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

namespace Notifications {

    inline void showError(QWidget* parent, const QString& text, int ms = 3500)
    {
        if (!parent) return;

        auto* n = new QLabel(parent);
        n->setText(text);
        n->setWordWrap(true);
        n->setStyleSheet(
            "QLabel {"
            "  background: #c0392b;"
            "  color: white;"
            "  padding: 10px 14px;"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "}");
        n->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        n->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        const int maxW = qMin(360, parent->width() / 3);
        n->setFixedWidth(maxW);
        n->adjustSize();

        const int margin = 20;
        n->move(parent->width() - n->width() - margin,
            parent->height() - n->height() - margin);
        n->show();
        n->raise();

        QTimer::singleShot(ms, n, &QLabel::deleteLater);
    }

    inline void showInfo(QWidget* parent, const QString& text, int ms = 3000)
    {
        if (!parent) return;

        auto* n = new QLabel(parent);
        n->setText(text);
        n->setWordWrap(true);
        n->setStyleSheet(
            "QLabel {"
            "  background: #2c7a4b;"
            "  color: white;"
            "  padding: 10px 14px;"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "}");
        n->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        n->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        const int maxW = qMin(360, parent->width() / 3);
        n->setFixedWidth(maxW);
        n->adjustSize();

        const int margin = 20;
        n->move(parent->width() - n->width() - margin,
            parent->height() - n->height() - margin);
        n->show();
        n->raise();

        QTimer::singleShot(ms, n, &QLabel::deleteLater);
    }

} // namespace Notifications

#endif // NOTIFICATIONS_H