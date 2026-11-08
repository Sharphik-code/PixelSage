#include "descriptionmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace {
const char* kDefaultPrompt =
    "Describe this image briefly and factually in 1-3 sentences. "
    "Do not invent details. Answer in English.";
const int kVersion = 1;
}

DescriptionManager::DescriptionManager(QObject* parent) : QObject(parent)
{
    ensureDataDir();
    m_filePath = m_dataDir + "/descriptions.json";
    m_prompt = QString::fromUtf8(kDefaultPrompt);
    initDefaultGroups();
}

void DescriptionManager::initDefaultGroups()
{
    if (m_groups.isEmpty()) {
        m_groups.insert("Избранные", QStringList());
    }
}


QString DescriptionManager::defaultPrompt()
{
    return QString::fromUtf8(kDefaultPrompt);
}

QString DescriptionManager::dataDir() const     { return m_dataDir; }
QString DescriptionManager::storagePath() const { return m_filePath; }
QString DescriptionManager::modelPath() const { return m_modelPath; }
void DescriptionManager::setModelPath(const QString& path)
{
    if (m_modelPath == path) return;
    m_modelPath = path;
    emit changed();
}

void DescriptionManager::ensureDataDir()
{
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA");
    m_dataDir = !appData.isEmpty()
                    ? appData + "/AIGallery"
                    : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
    QDir().mkpath(m_dataDir);
}

bool DescriptionManager::load()
{
    QFile f(m_filePath);
    if (!f.exists()) {
        qDebug() << "DescriptionManager: файл не найден, создаём новый:" << m_filePath;
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "DescriptionManager: не удалось открыть" << m_filePath;
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "DescriptionManager: ошибка JSON:" << err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonObject settings = root.value("settings").toObject();
    m_lastFolder = settings.value("last_folder").toString();
    m_modelPath = settings.value("model_path").toString();
    const QString p = settings.value("prompt").toString();
    if (!p.isEmpty()) m_prompt = p;
    const QString th = settings.value("theme").toString();
    if (!th.isEmpty()) m_theme = th;

    m_photos.clear();
    for (const QJsonValue& v : root.value("photos").toArray()) {
        if (v.isString()) m_photos.append(v.toString());
    }



    m_descriptions.clear();
    const QJsonObject desc = root.value("descriptions").toObject();
    for (auto it = desc.begin(); it != desc.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        Description d;
        d.filePath    = o.value("file_path").toString();
        d.fileName    = o.value("file_name").toString();
        d.description = o.value("description").toString();
        d.generatedAt = o.value("generated_at").toString();
        d.model       = o.value("model").toString();
        d.lang        = o.value("lang").toString();
        m_descriptions.insert(it.key(), d);
    }

    m_hiddenHashes.clear();
    for (const QJsonValue& v : root.value("hidden_hashes").toArray()) {
        if (v.isString()) m_hiddenHashes.insert(v.toString());
    }

    qDebug() << "DescriptionManager: загружено"
             << m_photos.size() << "фото,"
             << m_descriptions.size() << "описаний,"
             << m_hiddenHashes.size() << "скрытых, тема:" << m_theme;

    m_groups.clear();
    const QJsonObject groupsObj = root.value("groups").toObject();
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
        QStringList paths;
        for (const QJsonValue& v : it.value().toArray()) {
            if (v.isString()) paths.append(v.toString());
        }
        m_groups.insert(it.key(), paths);
    }
    initDefaultGroups();


    return true;
}

bool DescriptionManager::save()
{
    QJsonObject root;
    root["version"] = kVersion;

    QJsonObject settings;
    settings["last_folder"] = m_lastFolder;
    settings["model_path"] = m_modelPath;
    settings["prompt"] = m_prompt;
    settings["theme"] = m_theme;
    root["settings"] = settings;


    QJsonArray photosArr;
    for (const QString& p : m_photos) photosArr.append(p);
    root["photos"] = photosArr;

    QJsonObject descObj;
    for (auto it = m_descriptions.begin(); it != m_descriptions.end(); ++it) {
        const Description& d = it.value();
        QJsonObject o;
        o["file_path"]    = d.filePath;
        o["file_name"]    = d.fileName;
        o["description"]  = d.description;
        o["generated_at"] = d.generatedAt;
        o["model"]        = d.model;
        o["lang"]         = d.lang;
        descObj[it.key()] = o;
    }
    root["descriptions"] = descObj;

    QJsonArray hiddenArr;
    for (const QString& h : m_hiddenHashes) hiddenArr.append(h);
    root["hidden_hashes"] = hiddenArr;



    const QString tmp = m_filePath + ".tmp";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "DescriptionManager: не удалось записать" << tmp;
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

    if (QFile::exists(m_filePath)) QFile::remove(m_filePath);
    if (!QFile::rename(tmp, m_filePath)) {
        qWarning() << "DescriptionManager: не удалось заменить" << m_filePath;
        return false;
    }

    QJsonObject groupsObj;
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        QJsonArray arr;
        for (const QString& p : it.value()) arr.append(p);
        groupsObj[it.key()] = arr;
    }
    root["groups"] = groupsObj;


    return true;
}

QString DescriptionManager::lastFolder() const { return m_lastFolder; }
void DescriptionManager::setLastFolder(const QString& path)
{
    if (m_lastFolder == path) return;
    m_lastFolder = path;
    emit changed();
}

QString DescriptionManager::prompt() const { return m_prompt; }
void DescriptionManager::setPrompt(const QString& prompt)
{
    if (m_prompt == prompt) return;
    m_prompt = prompt;
    emit changed();
}

QString DescriptionManager::theme() const { return m_theme; }
void DescriptionManager::setTheme(const QString& theme)
{
    const QString t = theme.trimmed().toLower();
    if (t != "light" && t != "dark") {
        if (m_theme == "system") return;
        m_theme = "system";
        emit changed();
        return;
    }
    if (m_theme == t) return;
    m_theme = t;
    emit changed();
}

QStringList DescriptionManager::photos() const { return m_photos; }
void DescriptionManager::setPhotos(const QStringList& photos)
{
    m_photos = photos;
    emit changed();
}

void DescriptionManager::addPhoto(const QString& path)
{
    if (m_photos.contains(path)) return;
    m_photos.append(path);
    emit changed();
}

void DescriptionManager::removePhoto(const QString& path)
{
    if (m_photos.removeOne(path)) emit changed();
}

bool DescriptionManager::hasPhoto(const QString& path) const
{
    return m_photos.contains(path);
}

QString DescriptionManager::computeFileHash(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return QString();
    return QString::fromLatin1(hash.result().toHex());
}

QString DescriptionManager::hashForFile(const QString& filePath) const
{
    auto it = m_hashCache.constFind(filePath);
    if (it != m_hashCache.constEnd()) return it.value();

    const QString hash = computeFileHash(filePath);
    if (!hash.isEmpty()) m_hashCache.insert(filePath, hash);
    return hash;
}

bool DescriptionManager::hasDescription(const QString& filePath) const
{
    const QString h = hashForFile(filePath);
    return !h.isEmpty() && m_descriptions.contains(h);
}

DescriptionManager::Description DescriptionManager::description(const QString& filePath) const
{
    const QString h = hashForFile(filePath);
    if (h.isEmpty()) return Description{};
    return m_descriptions.value(h);
}

void DescriptionManager::setDescription(const QString& filePath, const Description& desc)
{
    const QString h = hashForFile(filePath);
    if (h.isEmpty()) {
        qWarning() << "DescriptionManager: не удалось вычислить хэш для" << filePath;
        return;
    }
    Description d = desc;
    if (d.fileName.isEmpty())    d.fileName    = QFileInfo(filePath).fileName();
    if (d.filePath.isEmpty())    d.filePath    = filePath;
    if (d.generatedAt.isEmpty()) d.generatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_descriptions.insert(h, d);
    emit changed();
}

void DescriptionManager::removeDescription(const QString& filePath)
{
    const QString h = hashForFile(filePath);
    if (h.isEmpty()) return;
    if (m_descriptions.remove(h) > 0) emit changed();
}

bool DescriptionManager::isHidden(const QString& filePath) const
{
    const QString h = hashForFile(filePath);
    return !h.isEmpty() && m_hiddenHashes.contains(h);
}

void DescriptionManager::hidePhoto(const QString& filePath)
{
    const QString h = hashForFile(filePath);
    if (h.isEmpty()) return;
    if (m_hiddenHashes.contains(h)) return;
    m_hiddenHashes.insert(h);
    emit changed();
}

void DescriptionManager::unhidePhoto(const QString& filePath)
{
    const QString h = hashForFile(filePath);
    if (h.isEmpty()) return;
    if (m_hiddenHashes.remove(h) > 0) emit changed();
}

void DescriptionManager::unhideAll()
{
    if (m_hiddenHashes.isEmpty()) return;
    m_hiddenHashes.clear();
    emit changed();
}

int DescriptionManager::hiddenCount() const { return m_hiddenHashes.size(); }

int DescriptionManager::describedCount() const   { return m_descriptions.size(); }
int DescriptionManager::totalPhotos() const      { return m_photos.size(); }

int DescriptionManager::undescribedCount() const
{
    int n = 0;
    for (const QString& p : m_photos) {
        if (!hasDescription(p)) ++n;
    }
    return n;
}

QStringList DescriptionManager::undescribedPhotos() const
{
    QStringList result;
    result.reserve(m_photos.size());
    for (const QString& p : m_photos) {
        if (!hasDescription(p)) result.append(p);
    }
    return result;
}

QStringList DescriptionManager::groups() const { return m_groups.keys(); }

QStringList DescriptionManager::groupPhotos(const QString& groupName) const {
    return m_groups.value(groupName);
}

void DescriptionManager::addGroup(const QString& groupName) {
    if (groupName.isEmpty() || m_groups.contains(groupName)) return;
    m_groups.insert(groupName, QStringList());
    emit changed();
}

void DescriptionManager::removeGroup(const QString& groupName) {
    if (groupName == "Избранные") return; // Запрещаем удалять избранное
    if (m_groups.remove(groupName) > 0) emit changed();
}

void DescriptionManager::renameGroup(const QString& oldName, const QString& newName) {
    if (oldName == "Избранные" || newName.isEmpty() || m_groups.contains(newName)) return;
    if (m_groups.contains(oldName)) {
        QStringList photos = m_groups.take(oldName);
        m_groups.insert(newName, photos);
        emit changed();
    }
}

void DescriptionManager::addPhotoToGroup(const QString& groupName, const QString& filePath) {
    if (!m_groups.contains(groupName)) return;
    QStringList& list = m_groups[groupName];
    if (!list.contains(filePath)) {
        list.append(filePath);
        emit changed();
    }
}

void DescriptionManager::removePhotoFromGroup(const QString& groupName, const QString& filePath) {
    if (!m_groups.contains(groupName)) return;
    if (m_groups[groupName].removeOne(filePath)) emit changed();
}

bool DescriptionManager::isPhotoInGroup(const QString& groupName, const QString& filePath) const {
    return m_groups.value(groupName).contains(filePath);
}