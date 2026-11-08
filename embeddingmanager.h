#ifndef EMBEDDINGMANAGER_H
#define EMBEDDINGMANAGER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QVector>
#include <QFile>
#include <QMutex>
#include <memory>
#include <string>
#include <onnxruntime_cxx_api.h>

class EmbeddingManager : public QObject
{
    Q_OBJECT
public:
    explicit EmbeddingManager(QObject* parent = nullptr);
    ~EmbeddingManager();

    // Загрузка модели UForm из директории (ожидает text_encoder.onnx + tokenizer.json)
    bool loadModel(const QString& modelDir);

    // Кодирование текста в вектор (256 float)
    // Возвращает пустой вектор при ошибке
    QVector<float> encode(const QString& text);

    // Сохранение и загрузка эмбеддингов на диск
    bool saveEmbeddings(const QString& filePath);
    bool loadEmbeddings(const QString& filePath);

    // Хранилище: SHA256 файла -> вектор
    void setEmbedding(const QString& fileHash, const QVector<float>& vec);
    bool hasEmbedding(const QString& fileHash) const;
    QVector<float> embedding(const QString& fileHash) const;
    void removeEmbedding(const QString& fileHash);

    int count() const;

    // Косинусная близость (оба вектора должны быть одной размерности)
    static float cosineSimilarity(const QVector<float>& a, const QVector<float>& b);

signals:
    void errorOccurred(const QString& error);


private:
    bool loadTokenizer(const QString& path);


    std::vector<int> tokenizeWord(const QString& word);
    Ort::Env m_env{ ORT_LOGGING_LEVEL_WARNING, "AIGallery" };
    std::unique_ptr<Ort::Session> m_session;
    std::vector<std::string> m_inputNames;
    std::vector<std::string> m_outputNames;
    std::vector<const char*> m_inputNamesPtr;
    std::vector<const char*> m_outputNamesPtr;
    std::vector<ONNXTensorElementDataType> m_inputTypes;


    // Токенизатор (упрощённый BERT WordPiece)
    QHash<QString, int> m_vocab;
    QHash<int, QString> m_idToToken;
    int m_clsId = 49406;
    int m_sepId = 49407;
    int m_padId = 0;
    int m_unkId = 49407;
    int m_maxLen = 77;   // было 64

    mutable QMutex m_mutex;
    QHash<QString, QVector<float>> m_embeddings;
};

#endif