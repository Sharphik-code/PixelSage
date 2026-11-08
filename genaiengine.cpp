#include "genaiengine.h"
#include "genaiworker.h"
#include "embeddingmanager.h"
#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QDebug>

GenAIEngine::GenAIEngine(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<QVector<float>>("QVector<float>");
    qRegisterMetaType<EmbeddingManager*>("EmbeddingManager*");

    m_workerThread = new QThread(this);
    m_worker = new GenAIWorker();
    m_worker->moveToThread(m_workerThread);
    m_worker->setStopFlag(&m_stopRequested);

    connect(m_worker, &GenAIWorker::modelLoaded, this, [this](bool success, const QString& error) {
        m_modelLoaded = success;
        if (!success) emit errorOccurred(error);
    });
    connect(m_worker, &GenAIWorker::tokenGenerated, this, &GenAIEngine::tokenGenerated);
    connect(m_worker, &GenAIWorker::generationFinished, this, &GenAIEngine::generationFinished);
    connect(m_worker, &GenAIWorker::captionGenerated, this, &GenAIEngine::captionGenerated);
    connect(m_worker, &GenAIWorker::descriptionEncoded, this, &GenAIEngine::descriptionEncoded);
    connect(m_worker, &GenAIWorker::queryTranslated, this, &GenAIEngine::queryTranslated);
    connect(m_worker, &GenAIWorker::errorOccurred, this, &GenAIEngine::errorOccurred);

    m_workerThread->start();
}

GenAIEngine::~GenAIEngine()
{
    stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(5000);
    }
    delete m_worker;
}

bool GenAIEngine::loadModel(const QString& modelPath)
{
    m_modelPath = modelPath;
    m_worker->setModelPath(modelPath);
    QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection);

    QEventLoop loop;
    connect(m_worker, &GenAIWorker::modelLoaded, &loop, &QEventLoop::quit);
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();
    return m_modelLoaded;
}

void GenAIEngine::generate(const QString& prompt)
{
    if (!m_modelLoaded) { emit errorOccurred("Модель не загружена."); return; }
    m_stopRequested.store(false);
    QMetaObject::invokeMethod(m_worker, "generate", Qt::QueuedConnection, Q_ARG(QString, prompt));
}

void GenAIEngine::generateFromImage(const QString& imagePath, const QString& prompt)
{
    if (!m_modelLoaded) { emit errorOccurred("Модель не загружена."); return; }
    m_stopRequested.store(false);
    QMetaObject::invokeMethod(m_worker, "generateFromImage", Qt::QueuedConnection, Q_ARG(QString, imagePath), Q_ARG(QString, prompt));
}

void GenAIEngine::generateCaption(const QString& filePath, const QString& prompt)
{
    if (!m_modelLoaded) { emit errorOccurred("Модель не загружена."); return; }
    m_stopRequested.store(false);
    QMetaObject::invokeMethod(m_worker, "generateCaption", Qt::QueuedConnection, Q_ARG(QString, filePath), Q_ARG(QString, prompt));
}

void GenAIEngine::stop()
{
    m_stopRequested.store(true);
}

bool GenAIEngine::isMultiModal() const
{
    return m_worker ? m_worker->isMultiModal() : false;
}

void GenAIEngine::setEmbeddingManager(EmbeddingManager* mgr)
{
    if (m_worker) QMetaObject::invokeMethod(m_worker, "setEmbeddingManager", Qt::QueuedConnection, Q_ARG(EmbeddingManager*, mgr));
}

void GenAIEngine::encodeDescription(const QString& fileHash, const QString& description)
{
    if (!m_worker) return;
    QMetaObject::invokeMethod(m_worker, "encodeDescription", Qt::QueuedConnection, Q_ARG(QString, fileHash), Q_ARG(QString, description));
}

void GenAIEngine::translateQuery(const QString& query)
{
    if (!m_modelLoaded) { emit errorOccurred("Модель не загружена."); return; }
    m_stopRequested.store(false);
    QMetaObject::invokeMethod(m_worker, "translateQuery", Qt::QueuedConnection, Q_ARG(QString, query));
}