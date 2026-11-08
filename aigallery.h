#ifndef AIGALLERY_H
#define AIGALLERY_H

#include <QMainWindow>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QStringList>
#include <QHash>
#include <QElapsedTimer>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class AIGallery; }
QT_END_NAMESPACE

class GenAIEngine;
class DescriptionManager;
class EmbeddingManager;

class AIGallery : public QMainWindow
{
    Q_OBJECT

public:
    explicit AIGallery(QWidget *parent = nullptr);
    ~AIGallery() override;

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void resizeEvent(QResizeEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void updateScale();
    void openPhotoFullscreen(const QString &path, int index);
    void showOverlayPhoto(int index);
    void rescanFolder();
    void handleSemanticSearch();
    void clearSearch();
    void onQueryTranslated(const QString& original, const QString& translated);
    void generateMissingEmbeddings();
    void toggleFullScreen();

private:
    void setupUI();
    void setupAIPanel();
    void setupSettingsPanel();
    void setAIPanelVisible(bool visible);
    void setSettingsPanelVisible(bool visible);
    void updateModelDependentUI();
    void setupBatchPanel();
    void setBatchPanelVisible(bool visible);
    void setBatchProgressHidden(bool hidden);
    void updateAIStatus(const QString& text, const QString& colorHex);

    void refreshGroupList(); // <-- Метод для обновления списка групп

    void updateTileForPath(const QString& filePath);
    void handleShowDescription(const QString& filePath);
    void handleRegenerate(const QString& filePath);
    void handleDeletePhoto(const QString& filePath);
    void handlePromptSettings();
    void applyTheme(const QString& themeName);
    void handleTileDrop(const QString& sourcePath, const QString& targetPath, bool insertAfter);

    void startBatchGeneration();
    void processNextInBatch();
    void cancelBatch();
    void finishBatch(bool cancelled);
    void updateBatchProgress();

    // --- UI ---
    QPoint m_dragPosition;
    QWidget *m_centralWidget = nullptr;
    QWidget *m_sidebar = nullptr;
    QWidget *m_titleBar = nullptr;
    QWidget *m_mainArea = nullptr;
    QWidget *m_settingsPanel = nullptr;
    QWidget *m_aiPanel = nullptr;

    QPushButton *m_btnBatch = nullptr;
    QPushButton *m_btnSettings = nullptr;
    QPushButton *m_aiToggleBtn = nullptr;

    // Элементы статуса ИИ и групп
    QLabel *m_aiStatusDot = nullptr;
    QLabel *m_aiStatusText = nullptr;
    QListWidget* m_groupList = nullptr;
    QPushButton* m_btnAddGroup = nullptr;
    QString m_currentGroupFilter;

    Ui::AIGallery *ui;
    QWidget *m_content = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_overlay = nullptr;
    QPushButton *m_overlayPrevBtn = nullptr;
    QPushButton *m_overlayNextBtn = nullptr;
    QPixmap m_overlayPix;
    int m_overlayIndex = -1;
    QStringList m_currentVisiblePhotos;
    QVector<QLabel*> m_tiles;
    QHash<QString, QPushButton*> m_showButtons;
    double m_scale = 1.0;
    QString folder;
    QStringList photos;

    DescriptionManager *m_descManager = nullptr;
    EmbeddingManager *m_embedMgr = nullptr;

    GenAIEngine *m_aiEngine = nullptr;
    QTextEdit *m_aiOutput = nullptr;
    QLineEdit *m_aiInput = nullptr;
    QPushButton *m_aiSendBtn = nullptr;
    QPushButton *m_aiStopBtn = nullptr;
    QPushButton *m_aiLoadBtn = nullptr;
    QPushButton *m_aiImageBtn = nullptr;
    QLabel *m_aiImagePreview = nullptr;
    QString m_selectedImagePath;

    QPushButton *m_batchCancelBtn = nullptr;
    QLabel *m_batchProgressLabel = nullptr;
    QProgressBar *m_batchProgressBar = nullptr;

    QStringList m_batchQueue;
    QString m_batchCurrentFile;
    int m_batchTotal = 0;
    int m_batchDone = 0;
    int m_batchSuccess = 0;
    int m_batchErrors = 0;
    int m_batchSkipped = 0;
    QStringList m_batchErrorFiles;
    QElapsedTimer m_batchTimer;
    bool m_batchActive = false;
    bool m_batchProgressHidden = false;

    QLineEdit* m_searchInput = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_searchClearBtn = nullptr;
    QStringList m_searchResults;
    QHash<QString, float> m_searchScores;

    QTimer* m_resizeTimer = nullptr;
    QString m_currentModelPath;
    std::function<void()> m_showPhoto;
};

#endif // AIGALLERY_H