#include "galleryai.h"
#include "./ui_galleryai.h"
#include <QLabel>
#include <QtWidgets>          // Библиотека для ui
#include <QDirIterator>
#include <QRandomGenerator>
#include <utility> // для std::as_const
#include <QScreen>
#include <QGuiApplication>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <functional>
#include <QSet>

namespace {
class ClickableLabel : public QLabel
{
public:
    using QLabel::QLabel;
    std::function<void()> onClick;

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (onClick) onClick();
        QLabel::mousePressEvent(e);
    }
};
} // namespace


void GalleryAI::updateScale()
{
    QScreen *scr = screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) { m_scale = 1.0; return; }

    // geometry() — весь экран целиком (без учёта панели задач),
    // это правильно для fullscreen-режима
    const QSize s = scr->geometry().size();

    const double sx = s.width()  / 1920.0;
    const double sy = s.height() / 1080.0;
    m_scale = std::min(sx, sy);

    qDebug() << "Экран:" << s << "scale =" << m_scale;

}
void GalleryAI::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    if (m_scroll)
        m_scroll->setGeometry(0, 60, width(), height() - 60);

    if (m_overlay) {
        m_overlay->setGeometry(rect());
        if (m_overlay->isVisible() && !m_overlayPix.isNull()) {
            m_overlay->setPixmap(m_overlayPix.scaled(m_overlay->size(),
                                                     Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation));
        }
    }
}
void GalleryAI::openPhotoFullscreen(const QString &path)
{
    QPixmap p(path);
    if (p.isNull()) return;

    m_overlayPix = p;

    m_overlay->setGeometry(rect());   // накрывает всё окно целиком
    m_overlay->setPixmap(m_overlayPix.scaled(m_overlay->size(),
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
    m_overlay->raise();
    m_overlay->show();
    m_overlay->setFocus();            // чтобы Esc анализировать

}

bool GalleryAI::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == m_overlay) {
        const bool isClick = (e->type() == QEvent::MouseButtonPress);
        const bool isEsc   = (e->type() == QEvent::KeyPress &&
                            static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape);

        if (isClick || isEsc) {
            m_overlay->hide();
            m_overlayPix = QPixmap();   // освобождаем память
            return true;                // событие поглощено
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

GalleryAI::GalleryAI(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GalleryAI)
{
    ui->setupUi(this);
    resize(1920, 1080);
    // контейнер, внутри которого будут лежать плитки
    m_content = new QWidget;
    m_content->setMinimumSize(1500, 10000);   // ширина/высота под ваш грид
    //реальный размер пересчитан в showPhoto() под число картинок

    // сама прокрутка
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_content);
    m_scroll->setWidgetResizable(true);
    m_scroll->setGeometry(0, 60, width(), height() - 60); // 60px оставили под кнопки
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_scroll->show();
    // оверлей для полноэкранного показа фото
    m_overlay = new QLabel(this);
    m_overlay->setAlignment(Qt::AlignCenter);
    m_overlay->setStyleSheet("background: black; color: white; font-size: 18px;");
    m_overlay->setFocusPolicy(Qt::StrongFocus);
    m_overlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_overlay->hide();
    m_overlay->installEventFilter(this);




    //показ фото
    auto showPhoto = [this]() {
        for (auto *l : std::as_const(m_tiles)){
            l->hide();
            l->setParent(nullptr);
            delete l;
        }
        m_tiles.clear();

        const double k = m_scale;

        if (photos.isEmpty()) {
            m_content->setMinimumSize(0, 0);   // ← добавили: нет фото — нет и прокрутки
            return;
        }

        QVector<QLabel*> labelRandom;
        qDebug() << folder;

        const int count = std::min<int>(photos.size(), 200);
        for (int i = 0; i < count; i++) {
            int yStepif2nd = 0;
            if (i % 4 == 0){
                yStepif2nd = 0;
            }
            else if (i % 4 == 1){
                yStepif2nd = 410;
            }
            else if (i % 4 == 2){
                yStepif2nd = 820;
            }
            else {
                yStepif2nd = 1230;
            }

            int xStepif2nd = 0;
            xStepif2nd = 40 + i/4 * 230;

            ClickableLabel *lbl = new ClickableLabel(m_content);
            lbl->setCursor(Qt::PointingHandCursor);
            lbl->raise();
            lbl->setAttribute(Qt::WA_Hover, true);
            lbl->setAttribute(Qt::WA_StyledBackground, true);
            lbl->show();

            lbl->resize(400 * k, 210 * k);
            lbl->move(yStepif2nd, xStepif2nd);
            lbl->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            lbl->setStyleSheet(
                "QLabel { border-radius: 5px; }"
                "QLabel:hover { background: #eaeaea; border: 2px solid #75aaff; }"
                );

            auto *txt = new QLabel(lbl);
            txt->setGeometry(0, 180, 400, 30);
            txt->setAlignment(Qt::AlignCenter);
            txt->setStyleSheet("color: black; border: none;");
            txt->show();
            txt->setAttribute(Qt::WA_TransparentForMouseEvents, true);

            labelRandom.append(lbl);
            m_tiles.append(lbl);

            // Берём фото по индексу — каждое ровно один раз
            const QString photoPath = photos.at(i);
            QFileInfo fi(photoPath);
            txt->setText(fi.fileName());

            QPixmap p(photoPath);
            if (p.isNull()) continue;

            lbl->setPixmap(p.scaled(400, 180,
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation));

            lbl->onClick = [this, photoPath]() {
                openPhotoFullscreen(photoPath);
            };

            qDebug() << "Фото создано:" << photoPath;
        }

        // ← добавили: пересчитываем размер контейнера под фактическое
        //            расположение плиток, чтобы скролл рос вместе с галереей
        int maxX = 0;
        int maxY = 0;
        for (auto *l : std::as_const(m_tiles)) {
            maxX = std::max(maxX, l->x() + l->width());
            maxY = std::max(maxY, l->y() + l->height());
        }
        // +40 по каждой оси — небольшой отступ, чтобы крайние плитки
        // не прилипали к границе области прокрутки
        m_content->setMinimumSize(maxX + 40, maxY + 40);
    };



    folder = "C:/Users/User/Downloads"; //папка


    auto *label = new QLabel("Галерея", m_content);
    label->resize(100, 100);
    label->move(20, 7);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setStyleSheet("color: #75aaff; "
                         "font-weight: bold; "       // Жирный шрифт (выделение)
                         "font-size: 18px;");        // Размер шрифта


    //выбор папки
    auto *btn1 = new QPushButton("Выбрать папку", this);
    connect(btn1, &QPushButton::clicked, this, [this, showPhoto]()  {
        qDebug() << "Кнопка 1 нажата";

        QString folderPath = QFileDialog::getExistingDirectory(
            this,
            tr("Выберите папку"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
            );

        if (folderPath.isEmpty()) {
            qDebug() << "Выбор отменён";
            return;
        }

        // запись в std::string (UTF-8, корректно для кириллицы)
        QByteArray utf8 = folderPath.toUtf8();
        selectedFolder.assign(utf8.constData(), utf8.size());

        folder = folderPath;
        rescanFolder();
        showPhoto();

        qDebug() << "QString:     " << folderPath;
        qDebug() << "std::string: " << QString::fromStdString(selectedFolder);
    });
    btn1->move(0, 0);
    btn1->setStyleSheet("QPushButton { color: #4771b5; }");


    auto *btn2 = new QPushButton("Тема", this);
    connect(btn2, &QPushButton::clicked, this, [this/*, labelImage*/]() {
        qDebug() << "Кнопка 2 нажата";
        //labelImage->setVisible(false);
    });
    btn2->move(100, 0);
    btn2->setStyleSheet("QPushButton { color: #4771b5; }");

    rescanFolder();
    //метка случайного фото
    /*auto *labelRandom = new QLabel(this);
    labelRandom->setScaledContents(true);
    labelRandom->resize(400, 200);
    labelRandom->move(0, 360);
    labelRandom->setStyleSheet("border: 1px solid gray;");*/

    showPhoto(); // показываем сразу при запуске

    // btn3 меняет случайную картинку
    auto *btn3 = new QPushButton("Обновить", this);
    connect(btn3, &QPushButton::clicked, this, [this, showPhoto]() {
        qDebug() << "Кнопка 3 нажата";
        rescanFolder();
        showPhoto();

    });
    btn3->move(200, 0);
    btn3->setStyleSheet("QPushButton { color: #4771b5; }");



    auto *enterTxt = new QLineEdit(this);
    enterTxt->move(330, 0);
    enterTxt->resize(300, 30);
}

void GalleryAI::rescanFolder()
{
    // Уже известные пути — чтобы не добавлять дубликаты
    QSet<QString> known;
    for (const QString &p : std::as_const(photos))
        known.insert(p);

    const int before = photos.size();

    QDirIterator it(folder,
                    { "*.jpg", "*.jpeg", "*.png", "*.bmp",
                     "*.gif", "*.jfif", "*.webp" },
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        if (!known.contains(p)) {   // добавляем только новые
            known.insert(p);
            photos << p;
        }
    }

    qDebug() << "Добавлено новых:" << (photos.size() - before)
             << "Всего:" << photos.size() << "в" << folder;
}

GalleryAI::~GalleryAI()
{
    delete ui;
}