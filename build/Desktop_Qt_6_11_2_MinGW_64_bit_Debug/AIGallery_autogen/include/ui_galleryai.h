/********************************************************************************
** Form generated from reading UI file 'galleryai.ui'
**
** Created by: Qt User Interface Compiler version 6.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_GALLERYAI_H
#define UI_GALLERYAI_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_GalleryAI
{
public:
    QWidget *centralwidget;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *GalleryAI)
    {
        if (GalleryAI->objectName().isEmpty())
            GalleryAI->setObjectName("GalleryAI");
        GalleryAI->resize(800, 600);
        centralwidget = new QWidget(GalleryAI);
        centralwidget->setObjectName("centralwidget");
        GalleryAI->setCentralWidget(centralwidget);
        menubar = new QMenuBar(GalleryAI);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 21));
        GalleryAI->setMenuBar(menubar);
        statusbar = new QStatusBar(GalleryAI);
        statusbar->setObjectName("statusbar");
        GalleryAI->setStatusBar(statusbar);

        retranslateUi(GalleryAI);

        QMetaObject::connectSlotsByName(GalleryAI);
    } // setupUi

    void retranslateUi(QMainWindow *GalleryAI)
    {
        GalleryAI->setWindowTitle(QCoreApplication::translate("GalleryAI", "GalleryAI", nullptr));
    } // retranslateUi

};

namespace Ui {
    class GalleryAI: public Ui_GalleryAI {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_GALLERYAI_H
