#include "genaiworker.h"
#include "embeddingmanager.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <exception>

static void checkOgaResult(OgaResult* result, const QString& context) {
    if (result != nullptr) {
        const char* errorMsg = OgaResultGetError(result);
        QString error = QString("Ошибка в %1: %2").arg(context, QString::fromUtf8(errorMsg));
        OgaDestroyResult(result);
        throw std::runtime_error(error.toStdString());
    }
}

GenAIWorker::GenAIWorker(QObject* parent) : QObject(parent) {}

GenAIWorker::~GenAIWorker()
{
    if (m_processor) { OgaDestroyMultiModalProcessor(m_processor); m_processor = nullptr; }
    if (m_tokenizer) { OgaDestroyTokenizer(m_tokenizer); m_tokenizer = nullptr; }
    if (m_model)     { OgaDestroyModel(m_model); m_model = nullptr; }
}

void GenAIWorker::setModelPath(const QString& path) { m_modelPath = path; }
void GenAIWorker::setStopFlag(std::atomic<bool>* flag) { m_stopFlag = flag; }
bool GenAIWorker::isMultiModal() const { return m_isMultiModal; }
void GenAIWorker::setEmbeddingManager(EmbeddingManager* mgr) { m_embedMgr = mgr; }

void GenAIWorker::loadModel()
{
    try {
        OgaResult* result = OgaCreateModel(m_modelPath.toStdString().c_str(), &m_model);
        checkOgaResult(result, "OgaCreateModel");

        result = OgaCreateTokenizer(m_model, &m_tokenizer);
        checkOgaResult(result, "OgaCreateTokenizer");

        result = OgaCreateMultiModalProcessor(m_model, &m_processor);
        if (result != nullptr) {
            OgaDestroyResult(result);
            result = nullptr;
            m_processor = nullptr;
            m_isMultiModal = false;
        } else {
            m_isMultiModal = true;
        }

        emit modelLoaded(true, QString());
    }
    catch (const std::exception& e) {
        emit modelLoaded(false, QString::fromUtf8(e.what()));
    }
}

void GenAIWorker::generate(const QString& prompt)
{
    if (!m_model || !m_tokenizer) {
        emit errorOccurred("Модель не загружена");
        return;
    }

    OgaSequences* sequences = nullptr;
    OgaGeneratorParams* params = nullptr;
    OgaGenerator* generator = nullptr;
    OgaTokenizerStream* stream = nullptr;

    try {
        OgaResult* result = OgaCreateGeneratorParams(m_model, &params);
        checkOgaResult(result, "OgaCreateGeneratorParams");

        OgaGeneratorParamsSetSearchNumber(params, "max_length", 2048);

        result = OgaCreateGenerator(m_model, params, &generator);
        checkOgaResult(result, "OgaCreateGenerator");

        QString templated = QString("<|user|>\n%1<|end|>\n<|assistant|>\n").arg(prompt);

        result = OgaCreateSequences(&sequences);
        checkOgaResult(result, "OgaCreateSequences");

        result = OgaTokenizerEncode(m_tokenizer, templated.toStdString().c_str(), sequences);
        checkOgaResult(result, "OgaTokenizerEncode");

        result = OgaGenerator_AppendTokenSequences(generator, sequences);
        checkOgaResult(result, "OgaGenerator_AppendTokenSequences");

        result = OgaCreateTokenizerStream(m_tokenizer, &stream);
        checkOgaResult(result, "OgaCreateTokenizerStream");

        QString fullText;
        while (!OgaGenerator_IsDone(generator)) {
            if (m_stopFlag && m_stopFlag->load()) break;

            result = OgaGenerator_GenerateNextToken(generator);
            checkOgaResult(result, "OgaGenerator_GenerateNextToken");

            const int32_t* tokens = nullptr;
            size_t tokenCount = 0;
            result = OgaGenerator_GetNextTokens(generator, &tokens, &tokenCount);
            checkOgaResult(result, "OgaGenerator_GetNextTokens");

            if (tokenCount > 0) {
                int32_t newToken = tokens[tokenCount - 1];
                const char* decoded = nullptr;
                result = OgaTokenizerStreamDecode(stream, newToken, &decoded);
                checkOgaResult(result, "OgaTokenizerStreamDecode");
                if (decoded) {
                    QString tokenStr = QString::fromUtf8(decoded);
                    fullText += tokenStr;
                    emit tokenGenerated(tokenStr);
                }
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
        emit generationFinished(fullText);
    }
    catch (const std::exception& e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }

    if (stream)    OgaDestroyTokenizerStream(stream);
    if (generator) OgaDestroyGenerator(generator);
    if (params)    OgaDestroyGeneratorParams(params);
    if (sequences) OgaDestroySequences(sequences);
}

void GenAIWorker::translateQuery(const QString& query)
{
    if (!m_model || !m_tokenizer) {
        emit errorOccurred("Модель не загружена для перевода.");
        return;
    }

    OgaSequences* sequences = nullptr;
    OgaGeneratorParams* params = nullptr;
    OgaGenerator* generator = nullptr;
    OgaTokenizerStream* stream = nullptr;

    try {
        OgaResult* result = OgaCreateGeneratorParams(m_model, &params);
        checkOgaResult(result, "OgaCreateGeneratorParams");

        OgaGeneratorParamsSetSearchNumber(params, "max_length", 256);
        OgaGeneratorParamsSetSearchNumber(params, "temperature", 0.1); // Минимум креатива
        OgaGeneratorParamsSetSearchNumber(params, "top_p", 0.9);

        result = OgaCreateGenerator(m_model, params, &generator);
        checkOgaResult(result, "OgaCreateGenerator");

        // Специальный системный промпт для перевода
        QString templated = QString(
                                "<|system|>\n"
                                "You are a translator. Translate the user's text to English. Output ONLY the translation, without any extra text or comments.\n"
                                "<|end|>\n"
                                "<|user|>\n"
                                "%1<|end|>\n"
                                "<|assistant|>\n"
                                ).arg(query);

        result = OgaCreateSequences(&sequences);
        checkOgaResult(result, "OgaCreateSequences");

        result = OgaTokenizerEncode(m_tokenizer, templated.toStdString().c_str(), sequences);
        checkOgaResult(result, "OgaTokenizerEncode");

        result = OgaGenerator_AppendTokenSequences(generator, sequences);
        checkOgaResult(result, "OgaGenerator_AppendTokenSequences");

        result = OgaCreateTokenizerStream(m_tokenizer, &stream);
        checkOgaResult(result, "OgaCreateTokenizerStream");

        QString fullText;
        while (!OgaGenerator_IsDone(generator)) {
            if (m_stopFlag && m_stopFlag->load()) break;

            result = OgaGenerator_GenerateNextToken(generator);
            checkOgaResult(result, "OgaGenerator_GenerateNextToken");

            const int32_t* tokens = nullptr;
            size_t tokenCount = 0;
            result = OgaGenerator_GetNextTokens(generator, &tokens, &tokenCount);
            checkOgaResult(result, "OgaGenerator_GetNextTokens");

            if (tokenCount > 0) {
                int32_t newToken = tokens[tokenCount - 1];
                const char* decoded = nullptr;
                result = OgaTokenizerStreamDecode(stream, newToken, &decoded);
                checkOgaResult(result, "OgaTokenizerStreamDecode");
                if (decoded) {
                    fullText += QString::fromUtf8(decoded);
                }
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }

        QString cleanTranslation = fullText.trimmed();
        emit queryTranslated(query, cleanTranslation);
    }
    catch (const std::exception& e) {
        emit errorOccurred(QString("Ошибка перевода: %1").arg(QString::fromUtf8(e.what())));
    }

    if (stream)    OgaDestroyTokenizerStream(stream);
    if (generator) OgaDestroyGenerator(generator);
    if (params)    OgaDestroyGeneratorParams(params);
    if (sequences) OgaDestroySequences(sequences);
}

void GenAIWorker::generateFromImage(const QString& imagePath, const QString& prompt)
{
    generateFromImageImpl(imagePath, prompt, QString());
}

void GenAIWorker::generateCaption(const QString& filePath, const QString& prompt)
{
    generateFromImageImpl(filePath, prompt, filePath);
}

void GenAIWorker::encodeDescription(const QString& fileHash, const QString& description)
{
    if (!m_embedMgr) {
        emit errorOccurred("EmbeddingManager не инициализирован");
        return;
    }
    const QVector<float> vec = m_embedMgr->encode(description);
    if (vec.isEmpty()) {
        emit errorOccurred(QString("Не удалось создать эмбеддинг для %1").arg(fileHash));
        return;
    }
    emit descriptionEncoded(fileHash, vec);
}

void GenAIWorker::generateFromImageImpl(const QString& imagePath, const QString& prompt, const QString& forFile)
{

    if (!m_isMultiModal) {
        if (forFile.isEmpty()) emit errorOccurred("Загруженная модель не поддерживает анализ изображений.");
        else emit errorOccurred(QString("Модель не поддерживает изображения (файл: %1)").arg(forFile));
        return;
    }

    const QString tempDir = QDir::tempPath();
    const QString asciiPath = tempDir + "/ort_genai_input.jpg";

    QFile::remove(asciiPath);
    if (!QFile::copy(imagePath, asciiPath)) {
        emit errorOccurred("Не удалось скопировать изображение: " + imagePath);
        return;
    }

    OgaGeneratorParams* params = nullptr;
    OgaGenerator* generator = nullptr;
    OgaTokenizerStream* stream = nullptr;
    OgaImages* images = nullptr;
    OgaStringArray* image_paths_str = nullptr;
    OgaNamedTensors* input_tensors = nullptr;

    try {
        const QByteArray asciiUtf8 = asciiPath.toUtf8();
        const char* image_paths[] = { asciiUtf8.constData() };

        OgaResult* result = OgaCreateStringArrayFromStrings(image_paths, 1, &image_paths_str);
        checkOgaResult(result, "OgaCreateStringArrayFromStrings");

        result = OgaLoadImages(image_paths_str, &images);
        checkOgaResult(result, "OgaLoadImages");

        QString templated = QString(
                                "<|system|>\n"
                                "You are a precise image captioning assistant. "
                                "Describe ONLY what you can directly see in the image. "
                                "Do NOT invent details. Answer in 1-3 short sentences. Answer in English.\n"
                                "<|end|>\n"
                                "<|user|>\n"
                                "<|image_1|>\n"
                                "%1<|end|>\n"
                                "<|assistant|>\n"
                                ).arg(prompt);

        const QByteArray promptUtf8 = templated.toUtf8();

        result = OgaProcessorProcessImages(m_processor, promptUtf8.constData(), images, &input_tensors);
        checkOgaResult(result, "OgaProcessorProcessImages");

        result = OgaCreateGeneratorParams(m_model, &params);
        checkOgaResult(result, "OgaCreateGeneratorParams");

        OgaGeneratorParamsSetSearchNumber(params, "max_length", 4096);
        OgaGeneratorParamsSetSearchNumber(params, "temperature", 0.4);
        OgaGeneratorParamsSetSearchNumber(params, "top_p", 0.85);
        OgaGeneratorParamsSetSearchNumber(params, "top_k", 40);
        OgaGeneratorParamsSetSearchNumber(params, "repetition_penalty", 1.2);

        result = OgaCreateGenerator(m_model, params, &generator);
        checkOgaResult(result, "OgaCreateGenerator");

        result = OgaGenerator_SetInputs(generator, input_tensors);
        checkOgaResult(result, "OgaGenerator_SetInputs");

        result = OgaCreateTokenizerStream(m_tokenizer, &stream);
        checkOgaResult(result, "OgaCreateTokenizerStream");

        QString fullText;
        while (!OgaGenerator_IsDone(generator)) {
            if (m_stopFlag && m_stopFlag->load()) break;

            result = OgaGenerator_GenerateNextToken(generator);
            checkOgaResult(result, "OgaGenerator_GenerateNextToken");

            const int32_t* tokens = nullptr;
            size_t tokenCount = 0;
            result = OgaGenerator_GetNextTokens(generator, &tokens, &tokenCount);
            checkOgaResult(result, "OgaGenerator_GetNextTokens");

            if (tokenCount > 0) {
                int32_t newToken = tokens[tokenCount - 1];
                const char* decoded = nullptr;
                result = OgaTokenizerStreamDecode(stream, newToken, &decoded);
                checkOgaResult(result, "OgaTokenizerStreamDecode");
                if (decoded) {
                    QString tokenStr = QString::fromUtf8(decoded);
                    fullText += tokenStr;
                    if (forFile.isEmpty()) emit tokenGenerated(tokenStr);
                }
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }

        if (forFile.isEmpty()) emit generationFinished(fullText);
        else emit captionGenerated(forFile, fullText.trimmed());
    }
    catch (const std::exception& e) {
        if (forFile.isEmpty()) emit errorOccurred(QString::fromUtf8(e.what()));
        else emit errorOccurred(QString("Ошибка генерации описания (%1): %2").arg(QFileInfo(forFile).fileName(), QString::fromUtf8(e.what())));
    }

    if (input_tensors)   OgaDestroyNamedTensors(input_tensors);
    if (image_paths_str) OgaDestroyStringArray(image_paths_str);
    if (images)          OgaDestroyImages(images);
    if (stream)          OgaDestroyTokenizerStream(stream);
    if (generator)       OgaDestroyGenerator(generator);
    if (params)          OgaDestroyGeneratorParams(params);

    QFile::remove(asciiPath);
}