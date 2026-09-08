#pragma once

#include <QObject>
#include <QVariantList>
#include <QStringList>

class SystemSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString refreshedAt READ refreshedAt NOTIFY dataChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY dataChanged)
    Q_PROPERTY(bool developerMode READ developerMode WRITE setDeveloperMode NOTIFY developerModeChanged)
    Q_PROPERTY(QString timeZoneId READ timeZoneId NOTIFY dateTimeChanged)
    Q_PROPERTY(QString localDateTime READ localDateTime NOTIFY dateTimeChanged)
    Q_PROPERTY(bool automaticTime READ automaticTime NOTIFY dateTimeChanged)
    Q_PROPERTY(bool automaticTimeAvailable READ automaticTimeAvailable NOTIFY dateTimeChanged)
    Q_PROPERTY(QStringList timeZoneChoices READ timeZoneChoices CONSTANT)
    Q_PROPERTY(bool twentyFourHour READ twentyFourHour WRITE setTwentyFourHour NOTIFY dateTimeChanged)

public:
    explicit SystemSettings(QObject *parent = nullptr);

    QString refreshedAt() const;
    QString statusMessage() const;
    bool developerMode() const;
    void setDeveloperMode(bool enabled);
    QString timeZoneId() const;
    QString localDateTime() const;
    bool automaticTime() const;
    bool automaticTimeAvailable() const;
    QStringList timeZoneChoices() const;
    bool twentyFourHour() const;
    void setTwentyFourHour(bool enabled);

    Q_INVOKABLE QStringList sectionIds() const;
    Q_INVOKABLE QString sectionTitle(const QString &sectionId) const;
    Q_INVOKABLE QString sectionDescription(const QString &sectionId) const;
    Q_INVOKABLE QVariantList rows(const QString &sectionId) const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool openHardwareDiagnostics();
    Q_INVOKABLE bool setTimeZone(const QString &zoneId);
    Q_INVOKABLE bool setAutomaticTime(bool enabled);

signals:
    void dataChanged();
    void developerModeChanged();
    void dateTimeChanged();

private:
    using Rows = QVariantList;

    static QVariantMap row(const QString &label,
                           const QString &value,
                           const QString &detail = {},
                           bool available = true,
                           bool writable = false,
                           bool technical = false);
    static QString readTextFile(const QString &path);
    static QMap<QString, QString> readKeyValueFile(const QString &path);
    static QString formatBytes(quint64 bytes);
    static QString processOutput(const QString &program,
                                 const QStringList &arguments,
                                 int timeoutMs = 1500);

    void collectAbout();
    void collectDisplay();
    void collectAppearance();
    void collectSound();
    void collectNetwork();
    void collectBluetooth();
    void collectPower();
    void collectStorage();
    void collectSystem();
    void collectDateTime();
    void collectHardwareDiagnostics();

    QHash<QString, Rows> m_rows;
    QString m_refreshedAt;
    QString m_statusMessage;
    bool m_developerMode = false;
    QString m_timeZoneId;
    QString m_localDateTime;
    bool m_automaticTime = false;
    bool m_automaticTimeAvailable = false;
    bool m_twentyFourHour = false;
};
