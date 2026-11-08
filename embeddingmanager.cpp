#include "embeddingmanager.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <exception>

EmbeddingManager::EmbeddingManager(QObject* parent)
    : QObject(parent)
{
}

EmbeddingManager::~EmbeddingManager()
{
    m_session.reset();
}

bool EmbeddingManager::loadModel(const QString& modelDir)
{
    const QString modelPath = modelDir + "/text_encoder.onnx";
    const QString tokenizerPath = modelDir + "/tokenizer.json";

    if (!QFile::exists(modelPath) || !QFile::exists(tokenizerPath)) {
        emit errorOccurred("Файлы модели не найдены в " + modelDir);
        return false;
    }

    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const std::wstring wModelPath = modelPath.toStdWString();
        m_session = std::make_unique<Ort::Session>(m_env, wModelPath.c_str(), opts);

        Ort::AllocatorWithDefaultOptions alloc;
        m_inputNames.clear();
        m_outputNames.clear();
        m_inputTypes.clear();

        for (size_t i = 0; i < m_session->GetInputCount(); ++i) {
            auto name = m_session->GetInputNameAllocated(i, alloc);
            m_inputNames.push_back(name.get());

            auto typeInfo = m_session->GetInputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            m_inputTypes.push_back(tensorInfo.GetElementType());
        }
        for (size_t i = 0; i < m_session->GetOutputCount(); ++i) {
            auto name = m_session->GetOutputNameAllocated(i, alloc);
            m_outputNames.push_back(name.get());
        }

        m_inputNamesPtr.clear();
        for (const auto& s : m_inputNames) m_inputNamesPtr.push_back(s.c_str());
        m_outputNamesPtr.clear();
        for (const auto& s : m_outputNames) m_outputNamesPtr.push_back(s.c_str());

        if (!loadTokenizer(tokenizerPath)) {
            emit errorOccurred("Не удалось загрузить токенизатор");
            return false;
        }

        return true;
    }
    catch (const Ort::Exception& e) {
        emit errorOccurred(QString("ONNX Runtime: %1").arg(e.what()));
        return false;
    }
}

bool EmbeddingManager::loadTokenizer(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;

    const QJsonObject root = doc.object();
    const QJsonObject model = root.value("model").toObject();

    m_vocab.clear();
    m_idToToken.clear();

    if (model.value("vocab").isArray()) {
        const QJsonArray arr = model.value("vocab").toArray();
        for (int i = 0; i < arr.size(); ++i) {
            if (!arr.at(i).isArray()) continue;
            const QJsonArray pair = arr.at(i).toArray();
            if (pair.isEmpty()) continue;
            const QString tok = pair.at(0).toString();
            if (tok.isEmpty()) continue;

            // В формате Unigram (XLM-R) ID токена - это его индекс в массиве!
            m_vocab.insert(tok, i);
            m_idToToken.insert(i, tok);
        }
    } else if (model.value("vocab").isObject()) {
        // Fallback для обычных словарей
        const QJsonObject o = model.value("vocab").toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            m_vocab.insert(it.key(), it.value().toInt());
            m_idToToken.insert(it.value().toInt(), it.key());
        }
    }

    if (m_vocab.isEmpty()) {
        qWarning() << "Не удалось распарсить vocab";
        return false;
    }

    // Стандартные ID для XLM-R (UForm)
    m_clsId = 0; m_padId = 1; m_sepId = 2; m_unkId = 3;

    const QJsonArray added = root.value("added_tokens").toArray();
    for (const auto& v : added) {
        const QJsonObject t = v.toObject();
        const QString content = t.value("content").toString();
        const int id = t.value("id").toInt();
        if (content == "<s>") m_clsId = id;
        else if (content == "</s>") m_sepId = id;
        else if (content == "<pad>") m_padId = id;
        else if (content == "<unk>") m_unkId = id;
    }

    m_maxLen = 77;
    qDebug() << "Tokenizer: словарь =" << m_vocab.size()
             << "CLS =" << m_clsId << "SEP =" << m_sepId
             << "PAD =" << m_padId << "UNK =" << m_unkId;
    return true;
}

std::vector<int> EmbeddingManager::tokenizeWord(const QString& word)
{
    std::vector<int> result;
    QString current = word;
    bool isFirst = true;

    while (!current.isEmpty()) {
        int foundLen = 0;
        int foundId = -1;

        for (int len = qMin(current.size(), 10); len >= 1; --len) {
            QString part = current.left(len);
            QString prefixed = isFirst ? QString(QChar(0x2581)) + part : part;

            auto it = m_vocab.constFind(prefixed);
            if (it != m_vocab.constEnd()) {
                foundId = it.value();
                foundLen = len;
                break;
            }
            it = m_vocab.constFind(part);
            if (it != m_vocab.constEnd()) {
                foundId = it.value();
                foundLen = len;
                break;
            }
        }

        if (foundLen > 0) {
            result.push_back(foundId);
            current = current.mid(foundLen);
            isFirst = false;
        } else {
            result.push_back(m_unkId);
            current = current.mid(1);
            isFirst = false;
        }
    }
    return result;
}

QVector<float> EmbeddingManager::encode(const QString& text)
{
    QMutexLocker lock(&m_mutex);
    if (!m_session) return {};

    try {
        const QString lower = text.toLower().trimmed();
        QStringList words;
        QString currentWord;
        for (const QChar& ch : lower) {
            if (ch.isLetterOrNumber() || ch == '\'' || ch == '-') currentWord.append(ch);
            else { if (!currentWord.isEmpty()) { words.append(currentWord); currentWord.clear(); } }
        }
        if (!currentWord.isEmpty()) words.append(currentWord);

        std::vector<int64_t> inputIds64;
        inputIds64.reserve(m_maxLen);
        inputIds64.push_back(m_clsId);

        for (const QString& w : words) {
            auto wordTokens = tokenizeWord(w);
            for (int id : wordTokens) {
                if (inputIds64.size() >= static_cast<size_t>(m_maxLen - 1)) break;
                inputIds64.push_back(id);
            }
        }
        inputIds64.push_back(m_sepId);

        const size_t actualLen = inputIds64.size();
        inputIds64.resize(m_maxLen, m_padId);

        std::vector<int64_t> attentionMask64(m_maxLen, 0);
        for (size_t i = 0; i < actualLen; ++i) attentionMask64[i] = 1;

        std::vector<int32_t> inputIds32(inputIds64.begin(), inputIds64.end());
        std::vector<int32_t> attentionMask32(attentionMask64.begin(), attentionMask64.end());

        std::vector<int64_t> shape = { 1, static_cast<int64_t>(m_maxLen) };
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<Ort::Value> inputs;

        if (m_inputTypes.size() > 0 && m_inputTypes[0] == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32) {
            inputs.push_back(Ort::Value::CreateTensor<int32_t>(memoryInfo, inputIds32.data(), inputIds32.size(), shape.data(), shape.size()));
        } else {
            inputs.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, inputIds64.data(), inputIds64.size(), shape.data(), shape.size()));
        }

        if (m_inputTypes.size() > 1 && m_inputTypes[1] == ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32) {
            inputs.push_back(Ort::Value::CreateTensor<int32_t>(memoryInfo, attentionMask32.data(), attentionMask32.size(), shape.data(), shape.size()));
        } else {
            inputs.push_back(Ort::Value::CreateTensor<int64_t>(memoryInfo, attentionMask64.data(), attentionMask64.size(), shape.data(), shape.size()));
        }

        auto outputs = m_session->Run(Ort::RunOptions{nullptr},
                                      m_inputNamesPtr.data(), inputs.data(), inputs.size(),
                                      m_outputNamesPtr.data(), m_outputNamesPtr.size());

        if (outputs.empty()) return {};

        auto& out = outputs[0];
        auto info = out.GetTensorTypeAndShapeInfo();
        auto shapeOut = info.GetShape();
        const float* data = out.GetTensorData<float>();

        QVector<float> vec;

        if (shapeOut.size() == 3) {
            int seqLen = static_cast<int>(shapeOut[1]);
            int dim = static_cast<int>(shapeOut[2]);
            vec.resize(dim);
            std::fill(vec.begin(), vec.end(), 0.0f);

            int validTokens = 0;
            for (int i = 0; i < seqLen; ++i) {
                if (attentionMask64[i] == 1) {
                    for (int d = 0; d < dim; ++d) vec[d] += data[i * dim + d];
                    validTokens++;
                }
            }
            if (validTokens > 0) {
                for (int d = 0; d < dim; ++d) vec[d] /= validTokens;
            }
        } else {
            const size_t total = info.GetElementCount();
            vec.resize(static_cast<int>(total));
            for (size_t i = 0; i < total; ++i) vec[static_cast<int>(i)] = data[i];
        }

        float norm = 0.0f;
        for (float v : vec) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 1e-8f) for (float& v : vec) v /= norm;

        return vec;
    }
    catch (const Ort::Exception& e) {
        qWarning() << "ONNX Exception (encode):" << e.what();
        return {};
    }
}

void EmbeddingManager::setEmbedding(const QString& fileHash, const QVector<float>& vec)
{
    QMutexLocker lock(&m_mutex);
    m_embeddings.insert(fileHash, vec);
}

bool EmbeddingManager::hasEmbedding(const QString& fileHash) const
{
    QMutexLocker lock(&m_mutex);
    return m_embeddings.contains(fileHash);
}

QVector<float> EmbeddingManager::embedding(const QString& fileHash) const
{
    QMutexLocker lock(&m_mutex);
    return m_embeddings.value(fileHash);
}

void EmbeddingManager::removeEmbedding(const QString& fileHash)
{
    QMutexLocker lock(&m_mutex);
    m_embeddings.remove(fileHash);
}

int EmbeddingManager::count() const
{
    QMutexLocker lock(&m_mutex);
    return m_embeddings.size();
}

float EmbeddingManager::cosineSimilarity(const QVector<float>& a, const QVector<float>& b)
{
    if (a.size() != b.size() || a.isEmpty()) return 0.0f;
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na < 1e-8f || nb < 1e-8f) return 0.0f;
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

bool EmbeddingManager::saveEmbeddings(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QDataStream out(&f);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);

    out.writeRawData("AIGE", 4);
    out << static_cast<quint32>(2); // Версия 2 (64-байтовый хэш)

    quint32 dim = 768;
    if (!m_embeddings.isEmpty()) dim = m_embeddings.begin().value().size();
    out << dim;
    out << static_cast<quint32>(m_embeddings.size());

    for (auto it = m_embeddings.begin(); it != m_embeddings.end(); ++it) {
        QByteArray hash64 = it.key().toUtf8().leftJustified(64, ' ', true);
        out.writeRawData(hash64.constData(), 64);

        const QVector<float>& v = it.value();
        for (float f : v) out << f;
    }
    return true;
}

bool EmbeddingManager::loadEmbeddings(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);
    QFile f(filePath);
    if (!f.exists()) return true;
    if (!f.open(QIODevice::ReadOnly)) return false;

    QDataStream in(&f);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    char magic[4];
    if (in.readRawData(magic, 4) != 4 || QByteArray(magic, 4) != "AIGE") return true;

    quint32 version = 0, dim = 0, count = 0;
    in >> version >> dim >> count;

    if (version != 2 || dim == 0) {
        qDebug() << "EmbeddingManager: старый формат файла, требуется перегенерация.";
        return true;
    }
    if (count == 0) return true;

    m_embeddings.clear();
    for (quint32 i = 0; i < count; ++i) {
        char hashBuf[64];
        if (in.readRawData(hashBuf, 64) != 64) break;
        const QString hash = QString::fromLatin1(QByteArray(hashBuf, 64)).trimmed();

        QVector<float> v(static_cast<int>(dim));
        for (quint32 j = 0; j < dim; ++j) {
            float val = 0.0f;
            in >> val;
            v[static_cast<int>(j)] = val;
        }
        m_embeddings.insert(hash, v);
    }
    qDebug() << "EmbeddingManager: загружено" << m_embeddings.size() << "эмбеддингов.";
    return true;
}