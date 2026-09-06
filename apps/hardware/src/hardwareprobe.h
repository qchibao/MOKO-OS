#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

class HardwareProbe final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString overallStatus READ overallStatus NOTIFY dataChanged)
    Q_PROPERTY(QString overallDisplayStatus READ overallDisplayStatus NOTIFY dataChanged)
    Q_PROPERTY(QString refreshedAt READ refreshedAt NOTIFY dataChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString exportDirectory READ exportDirectory WRITE setExportDirectory NOTIFY exportDirectoryChanged)

public:
    explicit HardwareProbe(QObject *parent = nullptr);

    QString overallStatus() const;
    QString overallDisplayStatus() const;
    QString refreshedAt() const;
    QString statusMessage() const;
    QString exportDirectory() const;
    void setExportDirectory(const QString &directory);

    Q_INVOKABLE QStringList sectionIds() const;
    Q_INVOKABLE QString sectionTitle(const QString &sectionId) const;
    Q_INVOKABLE QString sectionStatus(const QString &sectionId) const;
    Q_INVOKABLE QString sectionDisplayStatus(const QString &sectionId) const;
    Q_INVOKABLE QString sectionSummary(const QString &sectionId) const;
    Q_INVOKABLE QVariantList rows(const QString &sectionId) const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool exportJson();
    Q_INVOKABLE bool exportText();
    Q_INVOKABLE bool exportAll();
    Q_INVOKABLE bool exportJsonToPath(const QString &path);
    Q_INVOKABLE bool exportTextToPath(const QString &path);

    bool exportJsonToDirectory(const QString &directory);
    bool exportTextToDirectory(const QString &directory);
    QJsonObject report() const;
    QString manufacturer() const;
    QString model() const;
    QString activeRenderer() const;
    bool writableDiskDetected() const;

signals:
    void dataChanged();
    void statusMessageChanged();
    void exportDirectoryChanged();
    void exportWritten(const QString &format, const QString &path, qint64 bytes);

private:
    struct Section
    {
        QString title;
        QString status;
        QString summary;
        QVariantList rows;
    };

    static QVariantMap row(const QString &label,
                           const QString &value,
                           const QString &evidence = {},
                           bool available = true,
                           bool technical = false);
    static QString displayStatus(const QString &status);
    static QString readTextFile(const QString &path);
    static QMap<QString, QString> readKeyValueFile(const QString &path);
    static QString processOutput(const QString &program,
                                 const QStringList &arguments,
                                 int timeoutMs = 1600);
    static QString formatBytes(quint64 bytes);
    static QString vendorName(const QString &vendorId);
    static QString driverForDevice(const QString &devicePath);
    static QString detectOpenGlRenderer();
    static QString safeEventValue(QString value);

    void setSection(const QString &id,
                    const QString &title,
                    const QString &status,
                    const QString &summary,
                    const QVariantList &rows);
    void collectSystem();
    void collectCpu();
    void collectMemory();
    void collectGraphics();
    void collectStorage();
    void collectNetwork();
    void collectBluetooth();
    void collectAudio();
    void collectInput();
    void collectPower();
    void collectMacSpecific();
    void collectEvidence();
    void rebuildReport();
    void updateOverallStatus();
    bool writeReport(const QString &format, const QString &directory);
    bool writeReportPath(const QString &format, const QString &path);
    QByteArray textReport() const;
    void setStatusMessage(const QString &message);

    QStringList m_sectionOrder;
    QHash<QString, Section> m_sections;
    QJsonObject m_report;
    QJsonObject m_evidence;
    QString m_overallStatus = QStringLiteral("UNKNOWN");
    QString m_refreshedAt;
    QString m_statusMessage = QStringLiteral("Hardware scan has not run yet");
    QString m_exportDirectory;
    QString m_manufacturer;
    QString m_model;
    QString m_activeRenderer;
    bool m_writableDiskDetected = false;
};
