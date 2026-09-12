#pragma once

#include <QObject>
#include <QTimer>

class QDBusPendingCallWatcher;

class PowerKeyInhibitor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit PowerKeyInhibitor(QObject *parent = nullptr);
    ~PowerKeyInhibitor() override;

    bool enabled() const;
    bool active() const;

public slots:
    void setEnabled(bool enabled);

signals:
    void enabledChanged();
    void activeChanged();

private:
    void requestInhibitor();
    void scheduleRetry();
    void releaseInhibitor();

    QTimer m_retryTimer;
    QDBusPendingCallWatcher *m_request = nullptr;
    quint64 m_generation = 0;
    int m_inhibitorFd = -1;
    bool m_enabled = false;
};
