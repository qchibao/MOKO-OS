#pragma once

#include <QObject>
#include <QProcess>
#include <QHash>
#include <QString>

class PackageInstaller final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString packagePath READ packagePath NOTIFY packageChanged)
    Q_PROPERTY(QString packageType READ packageType NOTIFY packageChanged)
    Q_PROPERTY(QString packageName READ packageName NOTIFY packageChanged)
    Q_PROPERTY(QString version READ version NOTIFY packageChanged)
    Q_PROPERTY(QString architecture READ architecture NOTIFY packageChanged)
    Q_PROPERTY(QString installedSize READ installedSize NOTIFY packageChanged)
    Q_PROPERTY(QString fileSize READ fileSize NOTIFY packageChanged)
    Q_PROPERTY(QString description READ description NOTIFY packageChanged)
    Q_PROPERTY(QString maintainer READ maintainer NOTIFY packageChanged)
    Q_PROPERTY(QString trustMessage READ trustMessage NOTIFY packageChanged)
    Q_PROPERTY(QString packageHash READ packageHash NOTIFY packageChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QString developerDetails READ developerDetails NOTIFY stateChanged)
    Q_PROPERTY(bool installable READ installable NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)

public:
    explicit PackageInstaller(QObject *parent = nullptr);

    QString packagePath() const;
    QString packageType() const;
    QString packageName() const;
    QString version() const;
    QString architecture() const;
    QString installedSize() const;
    QString fileSize() const;
    QString description() const;
    QString maintainer() const;
    QString trustMessage() const;
    QString packageHash() const;
    QString state() const;
    QString statusMessage() const;
    QString developerDetails() const;
    bool installable() const;
    bool busy() const;

    Q_INVOKABLE bool inspect(const QString &path);
    Q_INVOKABLE bool install();
    Q_INVOKABLE void cancel();

signals:
    void packageChanged();
    void stateChanged();
    void installFinished(bool success, const QString &message);

private slots:
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);

private:
    void clearPackage();
    void setState(const QString &state, const QString &message);
    bool inspectDebianPackage(const QString &path);
    bool isSafeLocalPath(const QString &path, QString *canonicalPath) const;
    static QString formatBytes(qint64 bytes);
    static QString fieldValue(const QHash<QString, QString> &fields, const QString &key);

    QString m_packagePath;
    QString m_packageType;
    QString m_packageName;
    QString m_version;
    QString m_architecture;
    QString m_installedSize;
    QString m_fileSize;
    QString m_description;
    QString m_maintainer;
    QString m_trustMessage;
    QString m_packageHash;
    QString m_state = QStringLiteral("idle");
    QString m_statusMessage = QStringLiteral("Choose a Debian package to inspect.");
    QString m_developerDetails;
    QProcess *m_process = nullptr;
};
