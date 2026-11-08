#ifndef GENAIENGINE_H
#define GENAIENGINE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QThread>
#include <memory>
#include <atomic>

class GenAIWorker;
class EmbeddingManager;

class GenAIEngine : public QObject
{
    Q_OBJECT
public:
    explicit GenAIEngine(QObject* parent = nullptr);
    ~GenAIEngine() override;

    bool loadModel(const QString& modelPath);
    bool isModelLoaded() const { return m_modelLoaded; }
    bool isMultiModal() const;

    void generate(const QString& prompt);
    void generateFromImage(const QString& imagePath, const QString& prompt);
    void generateCaption(const QString& filePath, const QString& prompt);
    void stop();

    void setEmbeddingManager(EmbeddingManager* mgr);
    void encodeDescription(const QString& fileHash, const QString& description);
    void translateQuery(const QString& query); // <-- Новый метод

signals:
    void tokenGenerated(const QString& token);
    void generationFinished(const QString& fullText);
    void captionGenerated(const QString& filePath, const QString& description);
    void descriptionEncoded(const QString& fileHash, const QVector<float>& embedding);
    void queryTranslated(const QString& original, const QString& translated); // <-- Новый сигнал
    void errorOccurred(const QString& error);

private:
    QThread* m_workerThread = nullptr;
    GenAIWorker* m_worker = nullptr;
    std::atomic<bool> m_stopRequested{ false };
    QString m_modelPath;
    bool m_modelLoaded = false;
};

#endif