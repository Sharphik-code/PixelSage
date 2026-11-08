#ifndef GENAIWORKER_H
#define GENAIWORKER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include "ort_genai_c.h"

class EmbeddingManager;

class GenAIWorker : public QObject
{
    Q_OBJECT
public:
    explicit GenAIWorker(QObject* parent = nullptr);
    ~GenAIWorker() override;

    void setModelPath(const QString& path);
    void setStopFlag(std::atomic<bool>* flag);
    bool isMultiModal() const;

public slots:
    void loadModel();
    void generate(const QString& prompt);
    void generateFromImage(const QString& imagePath, const QString& prompt);
    void generateCaption(const QString& filePath, const QString& prompt);
    void setEmbeddingManager(EmbeddingManager* mgr);
    void encodeDescription(const QString& fileHash, const QString& description);
    void translateQuery(const QString& query);

signals:
    void modelLoaded(bool success, const QString& error);
    void tokenGenerated(const QString& token);
    void generationFinished(const QString& fullText);
    void captionGenerated(const QString& filePath, const QString& description);
    void descriptionEncoded(const QString& fileHash, const QVector<float>& embedding);
    void queryTranslated(const QString& original, const QString& translated);
    void errorOccurred(const QString& error);

private:
    void generateFromImageImpl(const QString& imagePath, const QString& prompt, const QString& forFile);

    QString m_modelPath;
    std::atomic<bool>* m_stopFlag = nullptr;
    EmbeddingManager* m_embedMgr = nullptr;

    OgaModel* m_model = nullptr;
    OgaTokenizer* m_tokenizer = nullptr;
    OgaMultiModalProcessor* m_processor = nullptr;
    bool m_isMultiModal = false;
};

#endif