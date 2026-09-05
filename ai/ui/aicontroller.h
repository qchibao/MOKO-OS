#pragma once

#include <QObject>

class QDBusServiceWatcher;

class AiController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
    Q_PROPERTY(bool providerAvailable READ providerAvailable NOTIFY providerAvailableChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString response READ response NOTIFY responseChanged)
    Q_PROPERTY(QString provider READ provider NOTIFY providerChanged)

public:
    explicit AiController(QObject *parent = nullptr);

    bool connected() const;
    bool processing() const;
    bool failed() const;
    bool providerAvailable() const;
    QString state() const;
    QString response() const;
    QString provider() const;

    Q_INVOKABLE void submit(const QString &prompt);
    Q_INVOKABLE void refreshConnection();

signals:
    void connectedChanged();
    void processingChanged();
    void failedChanged();
    void providerAvailableChanged();
    void stateChanged();
    void responseChanged();
    void providerChanged();

private:
    void setConnected(bool connected);
    void setProcessing(bool processing);
    void setFailed(bool failed);
    void setProviderAvailable(bool available);
    void setState(const QString &state);
    void setResponse(const QString &response);
    void setProvider(const QString &provider);

    QDBusServiceWatcher *m_serviceWatcher;
    bool m_connected = false;
    bool m_processing = false;
    bool m_failed = false;
    bool m_providerAvailable = false;
    quint64 m_connectionGeneration = 0;
    quint64 m_requestGeneration = 0;
    QString m_state = QStringLiteral("Connecting");
    QString m_response = QStringLiteral("MOKO AI is connecting to the local daemon.");
    QString m_provider;
};
