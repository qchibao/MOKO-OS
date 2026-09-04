#pragma once

#include <QObject>
#include <QVariantList>

class SystemSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString refreshedAt READ refreshedAt NOTIFY dataChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY dataChanged)

public:
    explicit SystemSettings(QObject *parent = nullptr);

    QString refreshedAt() const;
    QString statusMessage() const;

    Q_INVOKABLE QStringList sectionIds() const;
    Q_INVOKABLE QString sectionTitle(const QString &sectionId) const;
    Q_INVOKABLE QString sectionDescription(const QString &sectionId) const;
    Q_INVOKABLE QVariantList rows(const QString &sectionId) const;
    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    using Rows = QVariantList;

    static QVariantMap row(const QString &label,
                           const QString &value,
                           const QString &detail = {},
                           bool available = true,
                           bool writable = false);
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

    QHash<QString, Rows> m_rows;
    QString m_refreshedAt;
    QString m_statusMessage;
};
