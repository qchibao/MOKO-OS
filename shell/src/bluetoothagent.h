#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QString>

class BluetoothAgent final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

public:
    explicit BluetoothAgent(QObject *parent = nullptr);

    void beginPairing(const QString &devicePath, const QString &deviceName);
    void allowServiceAuthorization(const QString &devicePath, const QString &deviceName);

    bool promptVisible() const;
    QString promptKind() const;
    QString promptTitle() const;
    QString promptMessage() const;
    QString promptValue() const;
    bool promptNeedsInput() const;

    Q_INVOKABLE void respond(const QString &value, bool accepted);

signals:
    void promptChanged();
    void agentMessage(const QString &message);

public slots:
    void Release();
    QString RequestPinCode(const QDBusObjectPath &device);
    uint RequestPasskey(const QDBusObjectPath &device);
    void DisplayPinCode(const QDBusObjectPath &device, const QString &pinCode);
    void DisplayPasskey(const QDBusObjectPath &device, uint passkey, ushort entered);
    void RequestConfirmation(const QDBusObjectPath &device, uint passkey);
    void RequestAuthorization(const QDBusObjectPath &device);
    void AuthorizeService(const QDBusObjectPath &device, const QString &uuid);
    void Cancel();

private:
    enum class ReplyKind {
        None,
        PinCode,
        Passkey,
        Empty,
    };

    bool acceptsDevice(const QDBusObjectPath &device) const;
    void queuePrompt(const QDBusObjectPath &device,
                     ReplyKind replyKind,
                     const QString &kind,
                     const QString &title,
                     const QString &message,
                     const QString &value,
                     bool needsInput);
    void showInformation(const QDBusObjectPath &device,
                         const QString &title,
                         const QString &message,
                         const QString &value);
    void clearPrompt();

    QString m_allowedDevicePath;
    QString m_deviceName;
    QDBusMessage m_pendingMessage;
    ReplyKind m_replyKind = ReplyKind::None;
    QString m_promptKind;
    QString m_promptTitle;
    QString m_promptMessage;
    QString m_promptValue;
    bool m_promptVisible = false;
    bool m_promptNeedsInput = false;
};
