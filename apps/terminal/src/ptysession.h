#pragma once

#include <QObject>

class QSocketNotifier;
class QTimer;

class PtySession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)

public:
    explicit PtySession(QObject *parent = nullptr);
    ~PtySession() override;

    bool running() const;
    bool start(const QString &program, const QStringList &arguments = {});
    bool writeBytes(const QByteArray &bytes);
    void resize(int columns, int rows);
    void stop();

signals:
    void bytesReceived(const QByteArray &bytes);
    void runningChanged();
    void started(qint64 processId);
    void exited(int exitCode);
    void errorOccurred(const QString &message);

private slots:
    void readAvailable();
    void checkChild();

private:
    void closeMaster();

    int m_masterFd = -1;
    qint64 m_childPid = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_childTimer = nullptr;
    int m_columns = 80;
    int m_rows = 24;
};
