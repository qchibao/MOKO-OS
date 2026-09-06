#pragma once

#include <QObject>

class QProcess;

class ScreenshotController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastPath READ lastPath NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)

public:
    explicit ScreenshotController(QObject *parent = nullptr);

    bool busy() const;
    QString state() const;
    QString lastPath() const;
    QString statusMessage() const;

    Q_INVOKABLE bool captureFullScreen();

signals:
    void stateChanged();
    void screenshotSaved(const QString &path);
    void screenshotFailed(const QString &message);

private:
    void fail(const QString &message);
    QString screenshotDirectory() const;

    QProcess *m_process = nullptr;
    QString m_state = QStringLiteral("Ready");
    QString m_lastPath;
    QString m_statusMessage = QStringLiteral("Ready");
};
