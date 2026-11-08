#include "aigallery.h"
#include "./ui_aigallery.h"
#include "genaiengine.h"
#include "descriptionmanager.h"
#include "descriptiondialog.h"
#include "promptdialog.h"
#include "batchreportdialog.h"
#include "notifications.h"
#include "embeddingmanager.h"

#include <QLabel>
#include <QtWidgets>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <utility>
#include <QScreen>
#include <QGuiApplication>
#include <algorithm>
#include <QMouseEvent>
#include <QKeyEvent>
#include <functional>
#include <QSet>
#include <QTimer>
#include <QCloseEvent>
#include <QTextCursor>
#include <QFileDialog>
#include <QDebug>
#include <QStyleHints>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QScrollBar>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#endif

namespace {
const char* kTileMimeFormat = "application/x-aigallery-path";

class GroupListWidget : public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<void(QString, QString)> onPhotoDropped;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasFormat(kTileMimeFormat))
            event->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        if (event->mimeData()->hasFormat(kTileMimeFormat))
            event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        if (event->mimeData()->hasFormat(kTileMimeFormat)) {
            QListWidgetItem* item = itemAt(event->position().toPoint());
            if (item && onPhotoDropped) {
                QString groupName = item->text();
                QString path = QString::fromUtf8(event->mimeData()->data(kTileMimeFormat));
                onPhotoDropped(groupName, path);
            }
            event->acceptProposedAction();
        }
    }
};

class ClickableLabel : public QLabel
{
public:
    using QLabel::QLabel;
    std::function<void()> onClick;
    std::function<void(const QString&, bool)> onDrop;
    QString filePath;
    bool dragEnabled = true;
    bool dropAfter = false;

protected:
    void mousePressEvent(QMouseEvent *e) override {
        m_dragStarted = false;
        if (e->button() == Qt::LeftButton) m_dragStartPos = e->pos();
        QLabel::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (!dragEnabled || !(e->buttons() & Qt::LeftButton)) return;
        if ((e->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) return;
        m_dragStarted = true;
        auto* drag = new QDrag(this);
        auto* mime = new QMimeData();
        mime->setData(kTileMimeFormat, filePath.toUtf8());
        mime->setUrls({ QUrl::fromLocalFile(filePath) });
        drag->setMimeData(mime);
        QList<QPushButton*> btns = findChildren<QPushButton*>();
        for (auto* b : btns) b->hide();
        if (m_indicator) m_indicator->hide();
        QPixmap pix = grab().scaled(width() * 0.55, height() * 0.55, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        for (auto* b : btns) b->show();
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
        drag->exec(Qt::CopyAction);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && !m_dragStarted && onClick) onClick();
        QLabel::mouseReleaseEvent(e);
    }
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (!e->mimeData()->hasFormat(kTileMimeFormat) || e->source() == this) return;
        showDropIndicator(e->position().toPoint());
        e->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if (!e->mimeData()->hasFormat(kTileMimeFormat) || e->source() == this) return;
        showDropIndicator(e->position().toPoint());
        e->acceptProposedAction();
    }
    void dragLeaveEvent(QDragLeaveEvent* e) override { hideDropIndicator(); QLabel::dragLeaveEvent(e); }
    void dropEvent(QDropEvent* e) override {
        hideDropIndicator();
        if (!e->mimeData()->hasFormat(kTileMimeFormat)) return;
        const QString src = QString::fromUtf8(e->mimeData()->data(kTileMimeFormat));
        if (onDrop) onDrop(src, dropAfter);
        e->acceptProposedAction();
    }
private:
    void ensureIndicator() {
        if (m_indicator) return;
        m_indicator = new QLabel(this);
        m_indicator->setStyleSheet("background: #00d2ff;");
        m_indicator->setFixedWidth(4);
        m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    void showDropIndicator(const QPoint& pos) {
        dropAfter = (pos.x() > width() / 2);
        ensureIndicator();
        if (dropAfter) m_indicator->setGeometry(width() - 4, 0, 4, height());
        else m_indicator->setGeometry(0, 0, 4, height());
        m_indicator->raise(); m_indicator->show();
        setStyleSheet("QLabel { border-radius: 8px; background: rgba(0, 210, 255, 0.2); }");
    }
    void hideDropIndicator() {
        if (m_indicator) m_indicator->hide();
        setStyleSheet("QLabel { border-radius: 8px; } QLabel:hover { background: rgba(255,255,255,0.1); border: 2px solid #00d2ff; }");
    }
    QPoint m_dragStartPos;
    bool m_dragStarted = false;
    QLabel* m_indicator = nullptr;
};

QString formatRemaining(qint64 msRemaining) {
    const qint64 sec = msRemaining / 1000;
    if (sec < 60) return QString("~%1 сек").arg(sec);
    const qint64 min = sec / 60;
    const qint64 rem = sec % 60;
    if (rem == 0) return QString("~%1 мин").arg(min);
    return QString("~%1 мин %2 сек").arg(min).arg(rem);
}
}

// ============================================================================
// Окно и события
// ============================================================================
bool AIGallery::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        const LONG border_width = 8;
        RECT winrect;
        GetWindowRect(HWND(winId()), &winrect);
        long x = GET_X_LPARAM(msg->lParam);
        long y = GET_Y_LPARAM(msg->lParam);

        bool left = x >= winrect.left && x < winrect.left + border_width;
        bool right = x < winrect.right && x >= winrect.right - border_width;
        bool top = y >= winrect.top && y < winrect.top + border_width;
        bool bottom = y < winrect.bottom && y >= winrect.bottom - border_width;

        if (top && left) { *result = HTTOPLEFT; return true; }
        if (top && right) { *result = HTTOPRIGHT; return true; }
        if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
        if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
        if (left) { *result = HTLEFT; return true; }
        if (right) { *result = HTRIGHT; return true; }
        if (top) { *result = HTTOP; return true; }
        if (bottom) { *result = HTBOTTOM; return true; }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void AIGallery::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_titleBar && m_titleBar->geometry().contains(event->pos())) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }

    if (m_settingsPanel && m_settingsPanel->isVisible()) {
        if (!m_settingsPanel->geometry().contains(event->pos()) && !m_btnSettings->geometry().contains(event->pos())) {
            setSettingsPanelVisible(false);
        }
    }
    if (m_aiPanel && m_aiPanel->isVisible()) {
        if (!m_aiPanel->geometry().contains(event->pos()) && !m_aiToggleBtn->geometry().contains(event->pos())) {
            setAIPanelVisible(false);
        }
    }
    QMainWindow::mousePressEvent(event);
}

void AIGallery::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && m_titleBar && m_titleBar->geometry().contains(event->pos())) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
    QMainWindow::mouseMoveEvent(event);
}

void AIGallery::mouseReleaseEvent(QMouseEvent *event) {
    QMainWindow::mouseReleaseEvent(event);
}

void AIGallery::toggleFullScreen() {
    if (isFullScreen()) showNormal();
    else showFullScreen();
}

void AIGallery::updateScale() {
    QScreen *scr = screen(); if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) { m_scale = 1.0; return; }
    const QSize s = scr->geometry().size();
    m_scale = (std::min)(s.width() / 1920.0, s.height() / 1080.0);
}

void AIGallery::resizeEvent(QResizeEvent *e) {
    QMainWindow::resizeEvent(e);
    if (m_overlay) {
        m_overlay->setGeometry(rect());
        if (m_overlay->isVisible() && !m_overlayPix.isNull())
            m_overlay->setPixmap(m_overlayPix.scaled(m_overlay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        if (m_overlayPrevBtn) m_overlayPrevBtn->setGeometry(20, height()/2 - 40, 60, 80);
        if (m_overlayNextBtn) m_overlayNextBtn->setGeometry(width() - 80, height()/2 - 40, 60, 80);
    }

    if (m_resizeTimer) m_resizeTimer->start();
}

void AIGallery::showOverlayPhoto(int index) {
    if (index < 0 || index >= m_currentVisiblePhotos.size()) return;
    m_overlayIndex = index;
    QString path = m_currentVisiblePhotos.at(index);
    QPixmap p(path);
    if (p.isNull()) return;
    m_overlayPix = p;
    m_overlay->setPixmap(m_overlayPix.scaled(m_overlay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_overlay->setWindowTitle(QFileInfo(path).fileName());
}

void AIGallery::openPhotoFullscreen(const QString &path, int index) {
    m_overlayIndex = index;
    showOverlayPhoto(index);
    m_overlay->raise(); m_overlay->show(); m_overlay->setFocus();

    bool multi = m_currentVisiblePhotos.size() > 1;
    m_overlayPrevBtn->setVisible(multi);
    m_overlayNextBtn->setVisible(multi);
}

bool AIGallery::eventFilter(QObject *obj, QEvent *e) {
    if (obj == m_overlay && m_overlay->isVisible()) {
        if (e->type() == QEvent::MouseButtonPress) {
            m_overlay->hide(); m_overlayPix = QPixmap(); return true;
        }
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_Escape) {
                m_overlay->hide(); m_overlayPix = QPixmap(); return true;
            }
            if (ke->key() == Qt::Key_Left && m_overlayIndex > 0) {
                showOverlayPhoto(m_overlayIndex - 1); return true;
            }
            if (ke->key() == Qt::Key_Right && m_overlayIndex < m_currentVisiblePhotos.size() - 1) {
                showOverlayPhoto(m_overlayIndex + 1); return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, e);
}

// ============================================================================
// Конструктор
// ============================================================================
AIGallery::AIGallery(QWidget *parent) : QMainWindow(parent), ui(new Ui::AIGallery)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1400, 900);
    setMinimumSize(900, 600);

    m_descManager = new DescriptionManager(this);
    m_descManager->load();

    auto *saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true); saveTimer->setInterval(1000);
    connect(m_descManager, &DescriptionManager::changed, saveTimer, qOverload<>(&QTimer::start));
    connect(saveTimer, &QTimer::timeout, this, [this]() { m_descManager->save(); });

    m_embedMgr = new EmbeddingManager(this);
    {
        QString uformDir = QCoreApplication::applicationDirPath() + "/uform-multilingual";
        if (QFile::exists(uformDir + "/text_encoder.onnx") && m_embedMgr->loadModel(uformDir)) {
            m_embedMgr->loadEmbeddings(m_descManager->dataDir() + "/embeddings.bin");
        }
    }

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(200);
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() { if (m_showPhoto) m_showPhoto(); });

    setupUI();

    m_showPhoto = [this]() {
        if (!m_content || !m_scroll) return;
        int savedScroll = m_scroll->verticalScrollBar()->value();
        for (auto *l : std::as_const(m_tiles)) { l->hide(); l->setParent(nullptr); delete l; }
        m_tiles.clear(); m_showButtons.clear();

        m_currentVisiblePhotos.clear();

        if (!m_searchResults.isEmpty()) {
            m_currentVisiblePhotos = m_searchResults;
        } else {
            if (!m_currentGroupFilter.isEmpty()) {
                m_currentVisiblePhotos = m_descManager->groupPhotos(m_currentGroupFilter);
            } else {
                for (const QString& p : std::as_const(photos))
                    if (!m_descManager->isHidden(p)) m_currentVisiblePhotos.append(p);
            }
        }

        if (m_currentVisiblePhotos.isEmpty()) { m_content->setMinimumSize(0, 0); return; }

        int viewWidth = m_scroll->viewport()->width();
        int tileW = 240, tileH = 160, spacingX = 20, spacingY = 40, marginX = 20, marginY = 20;
        int cols = qMax(1, (viewWidth - marginX) / (tileW + spacingX));
        int count = (std::min)(static_cast<int>(m_currentVisiblePhotos.size()), 200);

        for (int i = 0; i < count; i++) {
            int col = i % cols;
            int row = i / cols;
            int x = marginX + col * (tileW + spacingX);
            int y = marginY + row * (tileH + spacingY);

            ClickableLabel *lbl = new ClickableLabel(m_content);
            lbl->setCursor(Qt::PointingHandCursor);
            lbl->setAttribute(Qt::WA_Hover, true);
            lbl->setAttribute(Qt::WA_StyledBackground, true);
            lbl->setAcceptDrops(true);
            lbl->show();
            lbl->resize(tileW, tileH);
            lbl->move(x, y);
            lbl->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            lbl->setStyleSheet("QLabel { border-radius: 8px; } QLabel:hover { background: rgba(255,255,255,0.1); border: 2px solid #00d2ff; }");

            auto *txt = new QLabel(lbl);
            txt->setGeometry(0, tileH - 24, tileW, 24);
            txt->setAlignment(Qt::AlignCenter);
            txt->setStyleSheet("color: #ffffff; background: rgba(0,0,0,150); border-bottom-left-radius: 8px; border-bottom-right-radius: 8px; font-size: 11px;");
            txt->show();
            txt->setAttribute(Qt::WA_TransparentForMouseEvents, true);

            m_tiles.append(lbl);
            const QString photoPath = m_currentVisiblePhotos.at(i);
            txt->setText(QFileInfo(photoPath).fileName());

            QPixmap p(photoPath);
            if (!p.isNull()) lbl->setPixmap(p.scaled(tileW, tileH - 24, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

            if (!m_searchResults.isEmpty() && m_searchScores.contains(photoPath)) {
                float sim = m_searchScores.value(photoPath);
                auto* scoreLbl = new QLabel(lbl);
                scoreLbl->setGeometry(4, 4, 40, 20);
                scoreLbl->setAlignment(Qt::AlignCenter);
                scoreLbl->setStyleSheet("QLabel { background: rgba(0, 210, 255, 220); color: white; border-radius: 4px; font-weight: bold; font-size: 10px; }");
                scoreLbl->setText(QString("%1%").arg(qRound(sim * 100)));
                scoreLbl->show();
            }

            lbl->filePath = photoPath;
            lbl->onClick = [this, photoPath, i]() { openPhotoFullscreen(photoPath, i); };
            lbl->onDrop = [this, photoPath](const QString& srcPath, bool after) {
                QTimer::singleShot(0, this, [this, srcPath, photoPath, after]() { handleTileDrop(srcPath, photoPath, after); });
            };

            int btnSize = 20, spacing = 2;
            int totalW = 5 * btnSize + 4 * spacing;
            int baseX = tileW - totalW - 4;
            int baseY = 4;

            auto styleBtn = [](QPushButton* b) {
                b->setStyleSheet("QPushButton { background: rgba(0,0,0,150); border: 1px solid rgba(255,255,255,50); border-radius: 4px; padding: 0; } QPushButton:hover { background: #00d2ff; border: none; }");
                b->setCursor(Qt::PointingHandCursor);
            };

            auto* btnShow = new QPushButton(lbl);
            btnShow->setGeometry(baseX, baseY, btnSize, btnSize);
            btnShow->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
            styleBtn(btnShow);
            connect(btnShow, &QPushButton::clicked, this, [this, photoPath]() { handleShowDescription(photoPath); });
            btnShow->show(); m_showButtons.insert(photoPath, btnShow);

            auto* btnRegen = new QPushButton(lbl);
            btnRegen->setGeometry(baseX + (btnSize + spacing), baseY, btnSize, btnSize);
            btnRegen->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
            styleBtn(btnRegen);
            connect(btnRegen, &QPushButton::clicked, this, [this, photoPath]() { handleRegenerate(photoPath); });
            btnRegen->show();

            auto* btnDel = new QPushButton(lbl);
            btnDel->setGeometry(baseX + 2 * (btnSize + spacing), baseY, btnSize, btnSize);
            btnDel->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
            styleBtn(btnDel);
            connect(btnDel, &QPushButton::clicked, this, [this, photoPath]() { handleDeletePhoto(photoPath); });
            btnDel->show();

            auto* statusLbl = new QLabel(lbl);
            statusLbl->setGeometry(baseX + 3 * (btnSize + spacing), baseY, btnSize, btnSize);
            statusLbl->setAlignment(Qt::AlignCenter);
            bool hasDesc = m_descManager->hasDescription(photoPath);
            if (hasDesc) {
                statusLbl->setText("✓");
                statusLbl->setStyleSheet("QLabel { background: rgba(0, 200, 100, 200); color: white; border-radius: 4px; font-weight: bold; font-size: 12px; }");
            } else {
                statusLbl->setText("—");
                statusLbl->setStyleSheet("QLabel { background: rgba(0,0,0,150); color: #888; border: 1px solid rgba(255,255,255,50); border-radius: 4px; font-size: 12px; }");
            }
            statusLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            statusLbl->show();

            auto* btnGroup = new QPushButton(lbl);
            btnGroup->setGeometry(baseX + 4 * (btnSize + spacing), baseY, btnSize, btnSize);
            btnGroup->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
            styleBtn(btnGroup);
            connect(btnGroup, &QPushButton::clicked, this, [this, photoPath, btnGroup, btnSize]() {
                QMenu menu(this);
                menu.setStyleSheet("QMenu { background: #2a2a40; color: white; border: 1px solid #555; } QMenu::item:selected { background: #00d2ff; }");
                QStringList groups = m_descManager->groups();
                for (const QString& g : groups) {
                    QAction* action = menu.addAction(g);
                    action->setCheckable(true);
                    action->setChecked(m_descManager->isPhotoInGroup(g, photoPath));
                    connect(action, &QAction::triggered, this, [this, g, photoPath, action]() {
                        if (action->isChecked()) m_descManager->addPhotoToGroup(g, photoPath);
                        else m_descManager->removePhotoFromGroup(g, photoPath);
                        if (m_currentGroupFilter == g && !action->isChecked()) {
                            if (m_showPhoto) m_showPhoto();
                        }
                    });
                }
                menu.exec(btnGroup->mapToGlobal(QPoint(0, btnSize)));
            });
            btnGroup->show();
        }

        int totalRows = (count + cols - 1) / cols;
        int totalHeight = marginY * 2 + totalRows * (tileH + spacingY);
        m_content->setMinimumSize(viewWidth, totalHeight);
        m_scroll->verticalScrollBar()->setValue(savedScroll);
    };

    QString savedFolder = m_descManager->lastFolder();
    folder = (!savedFolder.isEmpty() && QDir(savedFolder).exists()) ? savedFolder : "C:/Users/User/Downloads";
    photos = m_descManager->photos();

    setupAIPanel();
    setupSettingsPanel();
    setupBatchPanel();

    m_overlay = new QLabel(this);
    m_overlay->setAlignment(Qt::AlignCenter);
    m_overlay->setStyleSheet("background: rgba(0,0,0,0.9);");
    m_overlay->setFocusPolicy(Qt::StrongFocus);
    m_overlay->hide();
    m_overlay->installEventFilter(this);

    m_overlayPrevBtn = new QPushButton("❮", m_overlay);
    m_overlayPrevBtn->setStyleSheet("QPushButton { background: rgba(0,0,0,0.5); color: white; font-size: 30px; border: none; border-radius: 8px; } QPushButton:hover { background: rgba(0, 210, 255, 0.7); }");
    m_overlayPrevBtn->hide();
    connect(m_overlayPrevBtn, &QPushButton::clicked, this, [this]() { showOverlayPhoto(m_overlayIndex - 1); });

    m_overlayNextBtn = new QPushButton("❯", m_overlay);
    m_overlayNextBtn->setStyleSheet("QPushButton { background: rgba(0,0,0,0.5); color: white; font-size: 30px; border: none; border-radius: 8px; } QPushButton:hover { background: rgba(0, 210, 255, 0.7); }");
    m_overlayNextBtn->hide();
    connect(m_overlayNextBtn, &QPushButton::clicked, this, [this]() { showOverlayPhoto(m_overlayIndex + 1); });

    rescanFolder();
    if (m_showPhoto) m_showPhoto();
    applyTheme(m_descManager->theme());
    updateModelDependentUI();

    refreshGroupList();

    m_currentModelPath = m_descManager->modelPath();
    if (!m_currentModelPath.isEmpty() && QDir(m_currentModelPath).exists()) {
        QTimer::singleShot(500, this, [this]() {
            updateAIStatus("Загрузка ИИ...", "#ffb86c");
            m_batchProgressLabel->setText("Автозагрузка модели из памяти...");
            m_batchProgressLabel->show();
            m_batchProgressBar->setRange(0, 0);
            m_batchProgressBar->show();

            QTimer::singleShot(100, this, [this]() {
                bool ok = m_aiEngine->loadModel(m_currentModelPath);
                m_batchProgressBar->setRange(0, 100);
                m_batchProgressBar->hide();
                m_batchProgressLabel->hide();
                if (ok) updateModelDependentUI();
                else updateAIStatus("Ошибка загрузки ИИ", "#ff5555");
            });
        });
    }
}

// ============================================================================
// Создание UI
// ============================================================================
void AIGallery::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    m_centralWidget->setObjectName("CentralWidget");

    auto* mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_sidebar = new QWidget(m_centralWidget);
    m_sidebar->setFixedWidth(220);
    m_sidebar->setStyleSheet("background: rgba(0,0,0,0.2); border-top-left-radius: 12px; border-bottom-left-radius: 12px; border-right: 1px solid rgba(255,255,255,0.05);");

    auto* sideLayout = new QVBoxLayout(m_sidebar);
    sideLayout->setContentsMargins(20, 30, 20, 20);

    auto* logo = new QLabel("PixelSage", m_sidebar);
    logo->setStyleSheet("font-size: 24px; font-weight: bold; color: white; margin-bottom: 20px;");
    sideLayout->addWidget(logo);

    auto addNavBtn = [&](const QString& text) {
        auto* btn = new QPushButton(text, m_sidebar);
        btn->setStyleSheet("QPushButton { text-align: left; padding: 10px; border-radius: 8px; background: transparent; color: #cccccc; font-size: 14px; } QPushButton:hover { background: rgba(255,255,255,0.1); }");
        sideLayout->addWidget(btn);
        return btn;
    };

    auto* btnGallery = addNavBtn("  Галерея");
    connect(btnGallery, &QPushButton::clicked, this, [this]() {
        m_currentGroupFilter.clear();
        if (m_groupList) m_groupList->clearSelection();
        if (m_showPhoto) m_showPhoto();
    });

    m_btnSettings = addNavBtn("  Настройки");
    connect(m_btnSettings, &QPushButton::clicked, this, [this](){
        setSettingsPanelVisible(!m_settingsPanel->isVisible());
        setAIPanelVisible(false);
    });

    auto* groupHeaderLayout = new QHBoxLayout();
    auto* lblGroups = new QLabel("Группы", m_sidebar);
    lblGroups->setStyleSheet("color: #888; font-size: 12px; font-weight: bold; margin-top: 15px;");
    groupHeaderLayout->addWidget(lblGroups);
    groupHeaderLayout->addStretch();

    m_btnAddGroup = new QPushButton("+", m_sidebar);
    m_btnAddGroup->setFixedSize(20, 20);
    m_btnAddGroup->setStyleSheet("QPushButton { background: transparent; color: #00d2ff; font-weight: bold; border: none; font-size: 16px; } QPushButton:hover { color: white; }");
    connect(m_btnAddGroup, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, "Новая группа", "Название:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            m_descManager->addGroup(name);
            refreshGroupList();
        }
    });
    groupHeaderLayout->addWidget(m_btnAddGroup);
    sideLayout->addLayout(groupHeaderLayout);

    auto* groupListWidget = new GroupListWidget(m_sidebar);
    m_groupList = groupListWidget;
    m_groupList->setStyleSheet("QListWidget { background: transparent; border: none; color: #cccccc; } QListWidget::item { padding: 8px; border-radius: 4px; } QListWidget::item:selected { background: #00d2ff; color: white; } QListWidget::item:hover { background: rgba(255,255,255,0.1); }");
    m_groupList->setContextMenuPolicy(Qt::CustomContextMenu);
    sideLayout->addWidget(m_groupList);

    groupListWidget->onPhotoDropped = [this](QString groupName, QString path) {
        m_descManager->addPhotoToGroup(groupName, path);
        Notifications::showInfo(this, QString("Добавлено в «%1»").arg(groupName));
        if (m_currentGroupFilter == groupName && m_showPhoto) m_showPhoto();
    };

    connect(m_groupList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_currentGroupFilter = item->text();
        if (m_showPhoto) m_showPhoto();
    });

    connect(m_groupList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        QString oldName = item->text();
        if (oldName == "Избранные") return;
        bool ok;
        QString newName = QInputDialog::getText(this, "Переименовать", "Новое название:", QLineEdit::Normal, oldName, &ok);
        if (ok && !newName.isEmpty() && newName != oldName) {
            m_descManager->renameGroup(oldName, newName);
            if (m_currentGroupFilter == oldName) m_currentGroupFilter = newName;
            refreshGroupList();
        }
    });

    connect(m_groupList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_groupList->itemAt(pos);
        if (!item) return;
        QString groupName = item->text();

        QMenu menu(this);
        menu.setStyleSheet("QMenu { background: #2a2a40; color: white; border: 1px solid #555; } QMenu::item:selected { background: #00d2ff; }");

        QAction* actRename = menu.addAction("Переименовать");
        if (groupName == "Избранные") actRename->setEnabled(false);
        connect(actRename, &QAction::triggered, this, [this, item]() {
            emit m_groupList->itemDoubleClicked(item);
        });

        QAction* actDelete = menu.addAction("Удалить");
        if (groupName == "Избранные") actDelete->setEnabled(false);
        connect(actDelete, &QAction::triggered, this, [this, groupName]() {
            m_descManager->removeGroup(groupName);
            if (m_currentGroupFilter == groupName) {
                m_currentGroupFilter.clear();
                if (m_showPhoto) m_showPhoto();
            }
            refreshGroupList();
        });

        menu.exec(m_groupList->mapToGlobal(pos));
    });

    sideLayout->addStretch();

    auto* statusWidget = new QWidget(m_sidebar);
    auto* statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(8);

    m_aiStatusDot = new QLabel(statusWidget);
    m_aiStatusDot->setFixedSize(10, 10);
    m_aiStatusDot->setStyleSheet("background-color: #777; border-radius: 5px;");

    m_aiStatusText = new QLabel("ИИ не загружена", statusWidget);
    m_aiStatusText->setStyleSheet("color: #aaa; font-size: 12px;");

    statusLayout->addWidget(m_aiStatusDot);
    statusLayout->addWidget(m_aiStatusText);
    statusLayout->addStretch();

    sideLayout->addWidget(statusWidget);

    auto* rightContainer = new QWidget(m_centralWidget);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_titleBar = new QWidget(rightContainer);
    m_titleBar->setFixedHeight(40);
    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(0, 0, 10, 0);
    titleLayout->addStretch();

    auto* btnMin = new QPushButton("—", m_titleBar);
    auto* btnMax = new QPushButton("□", m_titleBar);
    auto* btnClose = new QPushButton("✕", m_titleBar);
    QString winBtnStyle = "QPushButton { background: transparent; color: white; font-size: 16px; padding: 5px 15px; border-radius: 4px; } QPushButton:hover { background: rgba(255,255,255,0.1); }";
    btnMin->setStyleSheet(winBtnStyle);
    btnMax->setStyleSheet(winBtnStyle);
    btnClose->setStyleSheet("QPushButton { background: transparent; color: white; font-size: 16px; padding: 5px 15px; border-radius: 4px; } QPushButton:hover { background: #e81123; }");

    connect(btnMin, &QPushButton::clicked, this, &AIGallery::showMinimized);
    connect(btnMax, &QPushButton::clicked, this, &AIGallery::toggleFullScreen);
    connect(btnClose, &QPushButton::clicked, this, &AIGallery::close);

    titleLayout->addWidget(btnMin);
    titleLayout->addWidget(btnMax);
    titleLayout->addWidget(btnClose);
    rightLayout->addWidget(m_titleBar);

    m_mainArea = new QWidget(rightContainer);
    auto* mainAreaLayout = new QVBoxLayout(m_mainArea);
    mainAreaLayout->setContentsMargins(20, 0, 20, 20);

    auto* topBar = new QWidget(m_mainArea);
    topBar->setFixedHeight(50);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);

    auto* btnFolder = new QPushButton("Выбрать папку", topBar);
    auto* btnRefresh = new QPushButton("Обновить", topBar);
    m_btnBatch = new QPushButton("Сгенерировать описания", topBar);
    m_aiToggleBtn = new QPushButton("Чат с ИИ", topBar);

    QString topBtnStyle = "QPushButton { background: rgba(255,255,255,0.05); color: #00d2ff; border: 1px solid rgba(0, 210, 255, 0.3); padding: 6px 12px; border-radius: 6px; } QPushButton:hover { background: rgba(0, 210, 255, 0.2); } QPushButton:disabled { color: #666; border-color: #444; background: rgba(0,0,0,0.1); }";
    for (auto* b : {btnFolder, btnRefresh, m_btnBatch, m_aiToggleBtn}) b->setStyleSheet(topBtnStyle);

    m_searchInput = new QLineEdit(topBar);
    m_searchInput->setPlaceholderText("Поиск...");
    m_searchInput->setStyleSheet("QLineEdit { background: rgba(0,0,0,0.3); color: white; border: 1px solid #555; padding: 6px 10px; border-radius: 6px; }");
    m_searchBtn = new QPushButton("Найти", topBar);
    m_searchBtn->setStyleSheet(topBtnStyle);
    m_searchClearBtn = new QPushButton("×", topBar);
    m_searchClearBtn->setStyleSheet("QPushButton { color: #ff5555; background: transparent; font-weight: bold; }");
    m_searchClearBtn->hide();

    topLayout->addWidget(btnFolder);
    topLayout->addWidget(btnRefresh);
    topLayout->addWidget(m_btnBatch);
    topLayout->addWidget(m_aiToggleBtn);
    topLayout->addStretch();
    topLayout->addWidget(m_searchInput);
    topLayout->addWidget(m_searchBtn);
    topLayout->addWidget(m_searchClearBtn);

    mainAreaLayout->addWidget(topBar);

    m_content = new QWidget;
    m_scroll = new QScrollArea(m_mainArea);
    m_scroll->setWidget(m_content);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setStyleSheet("QScrollArea { background: transparent; } QScrollBar:vertical { background: transparent; width: 10px; } QScrollBar::handle:vertical { background: #555; border-radius: 5px; }");
    m_scroll->viewport()->setStyleSheet("background: transparent;");
    mainAreaLayout->addWidget(m_scroll);

    rightLayout->addWidget(m_mainArea);
    mainLayout->addWidget(m_sidebar);
    mainLayout->addWidget(rightContainer);

    connect(btnFolder, &QPushButton::clicked, this, [this]() {
        QString folderPath = QFileDialog::getExistingDirectory(this, tr("Выберите папку"), QDir::homePath(), QFileDialog::ShowDirsOnly);
        if (folderPath.isEmpty()) return;
        folder = folderPath;
        m_currentGroupFilter.clear();
        if (m_groupList) m_groupList->clearSelection();
        clearSearch(); rescanFolder(); if (m_showPhoto) m_showPhoto();
    });
    connect(btnRefresh, &QPushButton::clicked, this, [this]() {
        clearSearch(); rescanFolder(); if (m_showPhoto) m_showPhoto();
    });
    connect(m_btnBatch, &QPushButton::clicked, this, &AIGallery::startBatchGeneration);
    connect(m_aiToggleBtn, &QPushButton::clicked, this, [this]() {
        setAIPanelVisible(!m_aiPanel->isVisible());
        setSettingsPanelVisible(false);
    });
    connect(m_searchBtn, &QPushButton::clicked, this, &AIGallery::handleSemanticSearch);
    connect(m_searchClearBtn, &QPushButton::clicked, this, &AIGallery::clearSearch);
    connect(m_searchInput, &QLineEdit::returnPressed, m_searchBtn, &QPushButton::click);
}

void AIGallery::refreshGroupList() {
    if (!m_groupList) return;
    m_groupList->clear();
    QStringList groups = m_descManager->groups();
    groups.sort();
    for (const QString& g : groups) {
        auto* item = new QListWidgetItem(g);
        item->setData(Qt::UserRole, g);
        m_groupList->addItem(item);
    }
}

void AIGallery::updateAIStatus(const QString& text, const QString& colorHex) {
    if (!m_aiStatusDot || !m_aiStatusText) return;
    m_aiStatusDot->setStyleSheet(QString("background-color: %1; border-radius: 5px;").arg(colorHex));
    m_aiStatusText->setText(text);
}

void AIGallery::updateModelDependentUI()
{
    bool loaded = m_aiEngine && m_aiEngine->isModelLoaded();
    if (m_btnBatch) m_btnBatch->setEnabled(loaded);
    if (m_searchBtn) m_searchBtn->setEnabled(loaded);
    if (m_aiSendBtn) m_aiSendBtn->setEnabled(loaded);
    if (m_aiImageBtn) m_aiImageBtn->setEnabled(loaded);

    if (loaded) {
        updateAIStatus("ИИ готова к работе", "#50fa7b");
    } else {
        updateAIStatus("ИИ не загружена", "#777");
    }
}

void AIGallery::setupAIPanel() {
    m_aiEngine = new GenAIEngine(this);
    if (m_embedMgr) m_aiEngine->setEmbeddingManager(m_embedMgr);

    m_aiPanel = new QWidget(m_mainArea);
    m_aiPanel->setObjectName("AIPanel");
    m_aiPanel->setGeometry(50, 60, 800, 500);
    m_aiPanel->setStyleSheet("QWidget#AIPanel { background: rgba(30,30,45,0.95); border: 1px solid #444; border-radius: 10px; }");
    m_aiPanel->hide();

    m_aiOutput = new QTextEdit(m_aiPanel);
    m_aiOutput->setGeometry(10, 10, 780, 400);
    m_aiOutput->setStyleSheet("QTextEdit { background: #1e1e2f; color: #d4d4d4; border: none; }");

    m_aiInput = new QLineEdit(m_aiPanel);
    m_aiInput->setGeometry(10, 420, 680, 30);
    m_aiInput->setStyleSheet("QLineEdit { background: #2a2a40; color: white; border: 1px solid #555; border-radius: 4px; padding: 4px; }");

    m_aiSendBtn = new QPushButton("Отправить", m_aiPanel);
    m_aiSendBtn->setGeometry(700, 420, 90, 30);
    m_aiSendBtn->setStyleSheet("QPushButton { background: #00d2ff; color: white; border: none; border-radius: 4px; } QPushButton:disabled { background: #555; color: #888; }");

    m_aiStopBtn = new QPushButton("Стоп", m_aiPanel);
    m_aiStopBtn->setGeometry(10, 460, 60, 30);
    m_aiStopBtn->setStyleSheet("QPushButton { background: #ff5555; color: white; border: none; border-radius: 4px; }");
    m_aiStopBtn->setEnabled(false);

    m_aiImagePreview = new QLabel(m_aiPanel);
    m_aiImagePreview->setGeometry(700, 10, 90, 90);
    m_aiImagePreview->setStyleSheet("background: #111; border: 1px dashed #555; color: #555;");
    m_aiImagePreview->setAlignment(Qt::AlignCenter);
    m_aiImagePreview->setText("Фото");

    m_aiImageBtn = new QPushButton("Выбрать фото", m_aiPanel);
    m_aiImageBtn->setGeometry(700, 110, 90, 30);
    m_aiImageBtn->setStyleSheet("QPushButton { background: #3c3c3c; color: white; border: none; border-radius: 4px; } QPushButton:disabled { color: #777; }");

    connect(m_aiEngine, &GenAIEngine::tokenGenerated, this, [this](const QString &token) {
        if (m_aiOutput) { m_aiOutput->moveCursor(QTextCursor::End); m_aiOutput->insertPlainText(token); m_aiOutput->ensureCursorVisible(); }
    });
    connect(m_aiEngine, &GenAIEngine::generationFinished, this, [this](const QString &) {
        if (m_aiSendBtn) m_aiSendBtn->setEnabled(true); if (m_aiStopBtn) m_aiStopBtn->setEnabled(false);
        updateAIStatus("ИИ готова к работе", "#50fa7b");
    });
    connect(m_aiEngine, &GenAIEngine::captionGenerated, this, [this](const QString& filePath, const QString& description) {
        DescriptionManager::Description d; d.filePath = filePath; d.fileName = QFileInfo(filePath).fileName(); d.description = description; d.model = "phi"; d.lang = "en";
        m_descManager->setDescription(filePath, d); updateTileForPath(filePath);
        if (m_embedMgr) { const QString hash = m_descManager->hashForFile(filePath); if (!hash.isEmpty()) m_aiEngine->encodeDescription(hash, description); }
        if (m_batchActive) {
            ++m_batchDone; ++m_batchSuccess;
            QTimer::singleShot(0, this, [this]() { processNextInBatch(); });
        }
        else {
            Notifications::showInfo(this, QString("Описание готово: %1").arg(d.fileName));
            updateAIStatus("ИИ готова к работе", "#50fa7b");
        }
    });
    connect(m_aiEngine, &GenAIEngine::errorOccurred, this, [this](const QString &error) {
        if (m_aiOutput) m_aiOutput->append(QString("<span style='color:red;'>%1</span>").arg(error.toHtmlEscaped()));
        if (m_aiSendBtn) m_aiSendBtn->setEnabled(true); if (m_aiStopBtn) m_aiStopBtn->setEnabled(false);
        if (m_batchActive) {
            ++m_batchDone; ++m_batchErrors;
            if (!m_batchCurrentFile.isEmpty()) m_batchErrorFiles.append(m_batchCurrentFile);
            QTimer::singleShot(0, this, [this]() { processNextInBatch(); });
        }
        else {
            Notifications::showError(this, error.left(200));
            updateAIStatus("Ошибка ИИ", "#ff5555");
            QTimer::singleShot(3000, this, [this]() { updateModelDependentUI(); });
        }
    });
    connect(m_aiEngine, &GenAIEngine::descriptionEncoded, this, [this](const QString& fileHash, const QVector<float>& vec) {
        if (m_embedMgr) { m_embedMgr->setEmbedding(fileHash, vec); m_embedMgr->saveEmbeddings(m_descManager->dataDir() + "/embeddings.bin"); }
    });
    connect(m_aiEngine, &GenAIEngine::queryTranslated, this, &AIGallery::onQueryTranslated);

    connect(m_aiSendBtn, &QPushButton::clicked, this, [this]() {
        QString prompt = m_aiInput->text().trimmed(); if (prompt.isEmpty()) return;
        m_aiOutput->clear(); m_aiInput->clear(); m_aiSendBtn->setEnabled(false); m_aiStopBtn->setEnabled(true);
        updateAIStatus("Генерация в чате...", "#ffb86c");
        m_aiEngine->generate(prompt);
    });
    connect(m_aiStopBtn, &QPushButton::clicked, this, [this]() {
        m_aiEngine->stop();
        m_aiSendBtn->setEnabled(true); m_aiStopBtn->setEnabled(false);
        updateAIStatus("ИИ готова к работе", "#50fa7b");
    });
}

void AIGallery::setAIPanelVisible(bool visible) {
    if (m_aiPanel) m_aiPanel->setVisible(visible);
}

void AIGallery::setupSettingsPanel() {
    m_settingsPanel = new QWidget(m_mainArea);
    m_settingsPanel->setObjectName("SettingsPanel");
    m_settingsPanel->setGeometry(50, 60, 600, 350);
    m_settingsPanel->setStyleSheet("QWidget#SettingsPanel { background: rgba(30,30,45,0.95); border: 1px solid #444; border-radius: 10px; }");
    m_settingsPanel->hide();

    auto* layout = new QVBoxLayout(m_settingsPanel);

    auto* title = new QLabel("Настройки", m_settingsPanel);
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    layout->addWidget(title);

    auto* themeLayout = new QHBoxLayout();
    themeLayout->addWidget(new QLabel("Тема оформления:", m_settingsPanel));
    auto* themeCombo = new QComboBox(m_settingsPanel);
    themeCombo->addItems({"Синяя (по умолчанию)", "Тёмная", "Светлая", "Как в системе", "Прозрачная (в будущем)"});
    themeCombo->setStyleSheet("background: #2a2a40; color: white; padding: 4px; border-radius: 4px;");
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        QStringList themes = {"blue", "dark", "light", "system", "transparent"};
        if (index >= 0 && index < themes.size()) { applyTheme(themes[index]); m_descManager->setTheme(themes[index]); }
    });
    themeLayout->addWidget(themeCombo);
    themeLayout->addStretch();
    layout->addLayout(themeLayout);

    auto* promptLayout = new QHBoxLayout();
    promptLayout->addWidget(new QLabel("Промпт для описаний:", m_settingsPanel));
    auto* btnPrompt = new QPushButton("Изменить...", m_settingsPanel);
    btnPrompt->setStyleSheet("background: #00d2ff; color: white; border: none; border-radius: 4px; padding: 5px 10px;");
    connect(btnPrompt, &QPushButton::clicked, this, &AIGallery::handlePromptSettings);
    promptLayout->addWidget(btnPrompt);
    promptLayout->addStretch();
    layout->addLayout(promptLayout);

    auto* modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel("Генеративная модель (Phi-3.5):", m_settingsPanel));
    m_aiLoadBtn = new QPushButton("Выбрать модель...", m_settingsPanel);
    m_aiLoadBtn->setStyleSheet("background: #00d2ff; color: white; border: none; border-radius: 4px; padding: 5px 10px;");
    connect(m_aiLoadBtn, &QPushButton::clicked, this, [this]() {
        QString modelPath = QFileDialog::getExistingDirectory(this, "Выберите папку с моделью", QDir::homePath(), QFileDialog::ShowDirsOnly);
        if (modelPath.isEmpty()) return;

        m_descManager->setModelPath(modelPath);
        m_currentModelPath = modelPath;

        updateAIStatus("Загрузка ИИ...", "#ffb86c");
        m_batchProgressLabel->setText("Загрузка модели... Пожалуйста, подождите.");
        m_batchProgressLabel->show();
        m_batchProgressBar->setRange(0, 0);
        m_batchProgressBar->show();

        QTimer::singleShot(100, this, [this, modelPath]() {
            bool ok = m_aiEngine->loadModel(modelPath);
            m_batchProgressBar->setRange(0, 100);
            m_batchProgressBar->hide();
            m_batchProgressLabel->hide();
            if (ok) Notifications::showInfo(this, "Модель успешно загружена");
            else Notifications::showError(this, "Ошибка загрузки модели");
            updateModelDependentUI();
        });
    });
    modelLayout->addWidget(m_aiLoadBtn);
    modelLayout->addStretch();
    layout->addLayout(modelLayout);

    auto* embLayout = new QHBoxLayout();
    embLayout->addWidget(new QLabel("Векторы для поиска:", m_settingsPanel));
    auto* btnGenEmbed = new QPushButton("Догенерировать эмбеддинги", m_settingsPanel);
    btnGenEmbed->setStyleSheet("background: #555; color: white; border: none; border-radius: 4px; padding: 5px 10px;");
    connect(btnGenEmbed, &QPushButton::clicked, this, &AIGallery::generateMissingEmbeddings);
    embLayout->addWidget(btnGenEmbed);
    embLayout->addStretch();
    layout->addLayout(embLayout);

    auto* btnClose = new QPushButton("Закрыть", m_settingsPanel);
    btnClose->setStyleSheet("background: #333; color: white; border-radius: 4px; padding: 5px;");
    connect(btnClose, &QPushButton::clicked, this, [this](){ setSettingsPanelVisible(false); });
    layout->addWidget(btnClose, 0, Qt::AlignRight);

    layout->addStretch();
}

void AIGallery::setSettingsPanelVisible(bool visible) {
    if (m_settingsPanel) m_settingsPanel->setVisible(visible);
}

void AIGallery::generateMissingEmbeddings() {
    if (!m_embedMgr || !m_aiEngine) {
        Notifications::showError(this, "Менеджеры не инициализированы");
        return;
    }

    int count = 0;
    for (const QString& p : std::as_const(photos)) {
        auto desc = m_descManager->description(p);
        if (desc.isValid()) {
            QString hash = m_descManager->hashForFile(p);
            if (!hash.isEmpty() && !m_embedMgr->hasEmbedding(hash)) {
                m_aiEngine->encodeDescription(hash, desc.description);
                count++;
            }
        }
    }

    if (count > 0) {
        Notifications::showInfo(this, QString("Запущена генерация %1 эмбеддингов...").arg(count));
    } else {
        Notifications::showInfo(this, "Все описания уже имеют эмбеддинги");
    }
}

void AIGallery::applyTheme(const QString& themeName) {
    QString bg = ""; QString text = "white";
    if (themeName == "dark") bg = "#1e1e1e";
    else if (themeName == "light") { bg = "#f5f5f5"; text = "#202020"; }
    else if (themeName == "system") {
        const auto scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark) bg = "#1e1e1e";
        else { bg = "#f5f5f5"; text = "#202020"; }
    } else if (themeName == "transparent") bg = "rgba(30, 30, 47, 150)";
    else bg = "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1e1e2f, stop:1 #2a2a40)";

    if (m_centralWidget) {
        m_centralWidget->setStyleSheet(QString(
                                           "QWidget#CentralWidget { background: %1; border-radius: 12px; border: 1px solid #444455; }"
                                           "QLabel { color: %2; }"
                                           ).arg(bg, text));
    }
}

void AIGallery::setupBatchPanel() {
    m_batchProgressLabel = new QLabel(this);
    m_batchProgressLabel->setGeometry(50, 850, 600, 22);
    m_batchProgressLabel->setStyleSheet("color: white; font-weight: bold;");
    m_batchProgressLabel->hide();

    m_batchProgressBar = new QProgressBar(this);
    m_batchProgressBar->setGeometry(50, 870, 400, 14);
    m_batchProgressBar->setStyleSheet("QProgressBar { background: #2d2d2d; border: 1px solid #555; border-radius: 7px; } QProgressBar::chunk { background: #00d2ff; border-radius: 7px; }");
    m_batchProgressBar->hide();

    m_batchCancelBtn = new QPushButton("Отмена", this);
    m_batchCancelBtn->setGeometry(460, 860, 80, 24);
    m_batchCancelBtn->setStyleSheet("QPushButton { background: #ff5555; color: white; border-radius: 4px; }");
    m_batchCancelBtn->hide();
    connect(m_batchCancelBtn, &QPushButton::clicked, this, &AIGallery::cancelBatch);
}

void AIGallery::setBatchPanelVisible(bool visible) {
    m_batchProgressLabel->setVisible(visible && !m_batchProgressHidden);
    m_batchProgressBar->setVisible(visible && !m_batchProgressHidden);
    m_batchCancelBtn->setVisible(visible && !m_batchProgressHidden && m_batchActive);
}
void AIGallery::setBatchProgressHidden(bool hidden) {
    m_batchProgressHidden = hidden;
    setBatchPanelVisible(m_batchActive);
}

void AIGallery::handleSemanticSearch() {
    if (!m_embedMgr || m_embedMgr->count() == 0) { Notifications::showError(this, "Нет эмбеддингов."); return; }
    QString q = m_searchInput->text().trimmed(); if (q.isEmpty()) { clearSearch(); return; }
    m_searchBtn->setEnabled(false); m_searchBtn->setText("...");
    updateAIStatus("Перевод запроса...", "#ffb86c");
    m_aiEngine->translateQuery(q);
}

void AIGallery::onQueryTranslated(const QString& original, const QString& translated) {
    m_searchBtn->setEnabled(true); m_searchBtn->setText("Найти");
    QVector<float> qVec = m_embedMgr->encode(translated);
    if (qVec.isEmpty()) {
        updateAIStatus("ИИ готова к работе", "#50fa7b");
        return;
    }

    m_searchScores.clear(); m_searchResults.clear();
    QVector<QPair<QString, float>> scored;

    for (const QString& p : photos) {
        QString h = m_descManager->hashForFile(p);
        if (!m_embedMgr->hasEmbedding(h)) continue;
        float sim = EmbeddingManager::cosineSimilarity(qVec, m_embedMgr->embedding(h));
        if (sim > 0.15f) { scored.append({p, sim}); m_searchScores.insert(p, sim); }
    }

    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.second > b.second; });
    for (auto& pr : scored) m_searchResults.append(pr.first);

    if (m_showPhoto) m_showPhoto();
    if (m_searchClearBtn) m_searchClearBtn->show();
    updateAIStatus("ИИ готова к работе", "#50fa7b");
}

void AIGallery::clearSearch() {
    m_searchResults.clear(); m_searchScores.clear(); m_searchInput->clear();
    if (m_showPhoto) m_showPhoto();
    if (m_searchClearBtn) m_searchClearBtn->hide();
}

void AIGallery::startBatchGeneration() {
    if (m_batchActive) return;
    if (!m_aiEngine->isModelLoaded()) return;

    m_batchQueue = m_descManager->undescribedPhotos();
    QStringList filtered;
    for (const QString& p : std::as_const(m_batchQueue))
        if (!m_descManager->isHidden(p)) filtered.append(p);
    m_batchQueue = filtered;

    if (m_batchQueue.isEmpty()) return;

    m_batchTotal = m_batchQueue.size();
    m_batchDone = 0; m_batchSuccess = 0; m_batchErrors = 0;
    m_batchErrorFiles.clear(); m_batchCurrentFile.clear();
    m_batchProgressHidden = false; m_batchActive = true;
    m_batchTimer.start();

    setBatchPanelVisible(true);
    updateBatchProgress();
    processNextInBatch();
}

void AIGallery::processNextInBatch() {
    if (!m_batchActive) return;
    if (m_batchQueue.isEmpty()) { finishBatch(false); return; }

    m_batchCurrentFile = m_batchQueue.takeFirst();
    updateBatchProgress();

    QString filePath = m_batchCurrentFile;
    QTimer::singleShot(50, this, [this, filePath]() {
        if (!m_batchActive) return;
        updateAIStatus("Генерация описаний...", "#ffb86c");
        m_aiEngine->generateCaption(filePath, m_descManager->prompt());
    });
}

void AIGallery::updateBatchProgress() {
    if (!m_batchActive) return;
    int percent = m_batchTotal > 0 ? qMin(100, m_batchDone * 100 / m_batchTotal) : 0;
    QString name = m_batchCurrentFile.isEmpty() ? "" : QFileInfo(m_batchCurrentFile).fileName();
    QString rem = "…";

    if (m_batchDone > 0 && m_batchTimer.isValid()) {
        double avgMs = (double)m_batchTimer.elapsed() / m_batchDone;
        qint64 msRem = (qint64)(avgMs * (m_batchTotal - m_batchDone));
        rem = formatRemaining(qMax<qint64>(0, msRem));
    }

    if (m_batchProgressLabel) {
        m_batchProgressLabel->setText(QString("Сейчас: %1 (%2/%3) (%4%) — осталось %5").arg(name).arg(m_batchDone).arg(m_batchTotal).arg(percent).arg(rem));
    }
    if (m_batchProgressBar) m_batchProgressBar->setValue(percent);
}

void AIGallery::cancelBatch() {
    if (!m_batchActive) return;
    m_aiEngine->stop();
    finishBatch(true);
}

void AIGallery::finishBatch(bool cancelled) {
    m_batchActive = false;
    BatchReportDialog::Report r;
    r.total = m_batchTotal; r.success = m_batchSuccess; r.errors = m_batchErrors;
    r.cancelled = cancelled; r.errorFiles = m_batchErrorFiles;
    r.elapsedMs = m_batchTimer.isValid() ? m_batchTimer.elapsed() : 0;

    m_batchQueue.clear(); m_batchCurrentFile.clear();
    setBatchPanelVisible(false);
    if (m_batchProgressBar) m_batchProgressBar->setValue(0);

    updateAIStatus("ИИ готова к работе", "#50fa7b");

    auto* dlg = new BatchReportDialog(r, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void AIGallery::handleShowDescription(const QString& filePath) {
    auto desc = m_descManager->description(filePath);
    if (!desc.isValid()) { Notifications::showError(this, "Нет описания"); return; }
    auto* dlg = new DescriptionDialog(desc, this);
    connect(dlg, &DescriptionDialog::regenerateRequested, this, [this, filePath]() { handleRegenerate(filePath); });
    dlg->show();
}

void AIGallery::handleRegenerate(const QString& filePath) {
    if (!m_aiEngine->isModelLoaded()) return;
    updateAIStatus("Генерация описания...", "#ffb86c");
    m_aiEngine->generateCaption(filePath, m_descManager->prompt());
}

void AIGallery::handleDeletePhoto(const QString& filePath) {
    QMessageBox mb(this); mb.setWindowTitle("Удаление"); mb.setText("Что сделать с фото?");
    auto* btnDisk = mb.addButton("Удалить с диска", QMessageBox::DestructiveRole);
    auto* btnHide = mb.addButton("Скрыть", QMessageBox::ActionRole);
    mb.addButton("Отмена", QMessageBox::RejectRole);
    mb.exec();
    if (mb.clickedButton() == btnDisk) { QFile::remove(filePath); photos.removeAll(filePath); m_descManager->setPhotos(photos); if(m_showPhoto) m_showPhoto(); }
    else if (mb.clickedButton() == btnHide) { m_descManager->hidePhoto(filePath); photos.removeAll(filePath); m_descManager->setPhotos(photos); if(m_showPhoto) m_showPhoto(); }
}

void AIGallery::handlePromptSettings() {
    PromptDialog dlg(m_descManager->prompt(), DescriptionManager::defaultPrompt(), this);
    if (dlg.exec() == QDialog::Accepted) m_descManager->setPrompt(dlg.prompt());
}

void AIGallery::updateTileForPath(const QString& filePath) {
    auto* btn = m_showButtons.value(filePath, nullptr);
    if (btn) {
        bool hasDesc = m_descManager->hasDescription(filePath);
        btn->setIcon(style()->standardIcon(hasDesc ? QStyle::SP_DialogApplyButton : QStyle::SP_MessageBoxInformation));
    }
}

void AIGallery::handleTileDrop(const QString& sourcePath, const QString& targetPath, bool insertAfter) {
    int srcIdx = photos.indexOf(sourcePath), tgtIdx = photos.indexOf(targetPath);
    if (srcIdx < 0 || tgtIdx < 0) return;
    int newIdx = insertAfter ? (tgtIdx + 1) : tgtIdx;
    if (srcIdx < newIdx) --newIdx;
    if (srcIdx != newIdx) { photos.move(srcIdx, newIdx); m_descManager->setPhotos(photos); if (m_showPhoto) m_showPhoto(); }
}

void AIGallery::rescanFolder() {
    QStringList existing; for (const QString &p : std::as_const(photos)) if (QFile::exists(p)) existing.append(p);
    photos = existing;
    QSet<QString> known; for (const QString &p : std::as_const(photos)) known.insert(p);
    QDirIterator it(folder, { "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.gif", "*.jfif", "*.webp" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString p = it.next(); if (!known.contains(p) && !m_descManager->isHidden(p)) { known.insert(p); photos.append(p); }
    }
    m_descManager->setPhotos(photos); m_descManager->setLastFolder(folder);
}

void AIGallery::closeEvent(QCloseEvent *e) {
    if (m_descManager) m_descManager->save();
    if (m_embedMgr) m_embedMgr->saveEmbeddings(m_descManager->dataDir() + "/embeddings.bin");
    QMainWindow::closeEvent(e);
}

AIGallery::~AIGallery() { delete ui; }