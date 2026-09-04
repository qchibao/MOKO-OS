#pragma once

#include <QObject>

class QDBusServiceWatcher;

class AiController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
    Q_PROPERTY(QString response READ response NOTIFY responseChanged)
    Q_PROPERTY(QString provider READ provider NOTIFY providerChanged)

public:
    explicit AiController(QObject *parent = nullptr);

    bool connected() const;
    bool processing() const;
    bool failed() const;
    QString response() const;
    QString provider() const;

    Q_INVOKABLE void submit(const QString &prompt);
    Q_INVOKABLE void refreshConnection();

signals:
    void connectedChanged();
    void processingChanged();
    void failedChanged();
    void responseChanged();
    void providerChanged();

private:
    void setConnected(bool connected);
    void setProcessing(bool processing);
    void setFailed(bool failed);
    void setResponse(const QString &response);
    void setProvider(const QString &provider);

    QDBusServiceWatcher *m_serviceWatcher;
    bool m_connected = false;
    bool m_processing = false;
    bool m_failed = false;
    QString m_response = QStringLiteral("MOKO AI is connecting to the local daemon.");
    QString m_provider;
};
