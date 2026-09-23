#ifndef GALLERYAI_H
#define GALLERYAI_H

#include <QMainWindow>
#include <QtWidgets>          // Библиотека для ui
#include <QDirIterator>       // ← добавили
#include <QRandomGenerator>

QT_BEGIN_NAMESPACE
namespace Ui {
class GalleryAI;
}
QT_END_NAMESPACE

class GalleryAI : public QMainWindow
{
    Q_OBJECT

public:
    explicit GalleryAI(QWidget *parent = nullptr);
    ~GalleryAI() override;

private:
    Ui::GalleryAI *ui;
    std::string selectedFolder;

    QString folder;               // текущая папка
    QStringList photos;           // список найденных файлов

    void rescanFolder();          // пересканировать папку в photos
    void showPhoto();    // отрисовать превью

    QVector<QLabel*> m_tiles;

    double m_scale = 1.0;
    void updateScale();

    QScrollArea *m_scroll = nullptr;
    QWidget     *m_content = nullptr;

    QLabel *m_overlay    = nullptr;   // затемнение поверх всего окна
    QPixmap m_overlayPix;             // исходная картинка (без масштаба)
    void openPhotoFullscreen(const QString &path);

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

};

#endif // GALLERYAI_H
