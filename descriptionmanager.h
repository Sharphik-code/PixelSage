#ifndef DESCRIPTIONMANAGER_H
#define DESCRIPTIONMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QSet>

class DescriptionManager : public QObject
{
    Q_OBJECT
public:
    struct Description {
        QString filePath;
        QString fileName;
        QString description;
        QString generatedAt;
        QString model;
        QString lang;

        bool isValid() const { return !description.isEmpty(); }
    };

    explicit DescriptionManager(QObject* parent = nullptr);
    QString m_modelPath;
    QString modelPath() const;
    void setModelPath(const QString& path);

    bool load();
    bool save();

    QString dataDir() const;
    QString storagePath() const;

    // Настройки
    QString lastFolder() const;
    void setLastFolder(const QString& path);

    QString prompt() const;
    void setPrompt(const QString& prompt);
    static QString defaultPrompt();

    // Тема
    QString theme() const;                    // "system" | "light" | "dark"
    void setTheme(const QString& theme);

    // Список фото
    QStringList photos() const;
    void setPhotos(const QStringList& photos);
    void addPhoto(const QString& path);
    void removePhoto(const QString& path);
    bool hasPhoto(const QString& path) const;

    // Описания
    static QString computeFileHash(const QString& filePath);
    QString hashForFile(const QString& filePath) const;
    bool hasDescription(const QString& filePath) const;
    Description description(const QString& filePath) const;
    void setDescription(const QString& filePath, const Description& desc);
    void removeDescription(const QString& filePath);

    // Скрытые
    bool isHidden(const QString& filePath) const;
    void hidePhoto(const QString& filePath);
    void unhidePhoto(const QString& filePath);
    void unhideAll();
    int hiddenCount() const;

    int describedCount() const;
    int totalPhotos() const;
    int undescribedCount() const;
    QStringList undescribedPhotos() const;

    QStringList groups() const;
    QStringList groupPhotos(const QString& groupName) const;
    void addGroup(const QString& groupName);
    void removeGroup(const QString& groupName);
    void renameGroup(const QString& oldName, const QString& newName);
    void addPhotoToGroup(const QString& groupName, const QString& filePath);
    void removePhotoFromGroup(const QString& groupName, const QString& filePath);
    bool isPhotoInGroup(const QString& groupName, const QString& filePath) const;

signals:
    void changed();

private:
    void ensureDataDir();

    QString m_dataDir;
    QString m_filePath;
    QString m_lastFolder;
    QString m_prompt;
    QString m_theme = "system";              // <-- новое
    QStringList m_photos;
    QHash<QString, Description> m_descriptions;
    QSet<QString> m_hiddenHashes;
    mutable QHash<QString, QString> m_hashCache;

    QHash<QString, QStringList> m_groups;
    void initDefaultGroups();
};

#endif // DESCRIPTIONMANAGER_H