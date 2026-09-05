#include "bluetoothagent.h"

#include <QDBusConnection>

BluetoothAgent::BluetoothAgent(QObject *parent)
    : QObject(parent)
{
}

void BluetoothAgent::beginPairing(const QString &devicePath, const QString &deviceName)
{
    m_allowedDevicePath = devicePath;
    m_deviceName = deviceName;
    clearPrompt();
}

void BluetoothAgent::allowServiceAuthorization(const QString &devicePath,
                                                const QString &deviceName)
{
    m_allowedDevicePath = devicePath;
    m_deviceName = deviceName;
}

bool BluetoothAgent::promptVisible() const
{
    return m_promptVisible;
}

QString BluetoothAgent::promptKind() const
{
    return m_promptKind;
}

QString BluetoothAgent::promptTitle() const
{
    return m_promptTitle;
}

QString BluetoothAgent::promptMessage() const
{
    return m_promptMessage;
}

QString BluetoothAgent::promptValue() const
{
    return m_promptValue;
}

bool BluetoothAgent::promptNeedsInput() const
{
    return m_promptNeedsInput;
}

void BluetoothAgent::respond(const QString &value, bool accepted)
{
    if (m_replyKind == ReplyKind::None || m_pendingMessage.type() == QDBusMessage::InvalidMessage) {
        clearPrompt();
        return;
    }

    QDBusMessage reply;
    if (!accepted) {
        reply = m_pendingMessage.createErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                                                  QStringLiteral("Pairing was rejected by the user"));
    } else if (m_replyKind == ReplyKind::PinCode) {
        if (value.isEmpty() || value.size() > 16) {
            emit agentMessage(QStringLiteral("Enter a valid Bluetooth PIN."));
            return;
        }
        reply = m_pendingMessage.createReply(QVariantList{value});
    } else if (m_replyKind == ReplyKind::Passkey) {
        bool ok = false;
        const uint passkey = value.toUInt(&ok);
        if (!ok || passkey > 999999) {
            emit agentMessage(QStringLiteral("Enter a six-digit Bluetooth passkey."));
            return;
        }
        reply = m_pendingMessage.createReply(QVariantList{passkey});
    } else {
        reply = m_pendingMessage.createReply();
    }
    QDBusConnection::systemBus().send(reply);
    clearPrompt();
}

void BluetoothAgent::Release()
{
    clearPrompt();
    m_allowedDevicePath.clear();
    m_deviceName.clear();
}

QString BluetoothAgent::RequestPinCode(const QDBusObjectPath &device)
{
    if (!acceptsDevice(device)) {
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Unexpected Bluetooth device"));
        return {};
    }
    queuePrompt(device,
                ReplyKind::PinCode,
                QStringLiteral("pin"),
                QStringLiteral("Bluetooth PIN"),
                QStringLiteral("Enter the PIN shown by %1.").arg(m_deviceName),
                {},
                true);
    return {};
}

uint BluetoothAgent::RequestPasskey(const QDBusObjectPath &device)
{
    if (!acceptsDevice(device)) {
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Unexpected Bluetooth device"));
        return 0;
    }
    queuePrompt(device,
                ReplyKind::Passkey,
                QStringLiteral("passkey"),
                QStringLiteral("Bluetooth passkey"),
                QStringLiteral("Enter the passkey for %1.").arg(m_deviceName),
                {},
                true);
    return 0;
}

void BluetoothAgent::DisplayPinCode(const QDBusObjectPath &device, const QString &pinCode)
{
    showInformation(device,
                    QStringLiteral("Bluetooth PIN"),
                    QStringLiteral("Type this PIN on %1.").arg(m_deviceName),
                    pinCode);
}

void BluetoothAgent::DisplayPasskey(const QDBusObjectPath &device, uint passkey, ushort entered)
{
    Q_UNUSED(entered)
    showInformation(device,
                    QStringLiteral("Bluetooth passkey"),
                    QStringLiteral("Type this passkey on %1.").arg(m_deviceName),
                    QStringLiteral("%1").arg(passkey, 6, 10, QLatin1Char('0')));
}

void BluetoothAgent::RequestConfirmation(const QDBusObjectPath &device, uint passkey)
{
    if (!acceptsDevice(device)) {
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Unexpected Bluetooth device"));
        return;
    }
    const QString value = QStringLiteral("%1").arg(passkey, 6, 10, QLatin1Char('0'));
    queuePrompt(device,
                ReplyKind::Empty,
                QStringLiteral("confirmation"),
                QStringLiteral("Confirm Bluetooth code"),
                QStringLiteral("Does this code match %1?").arg(m_deviceName),
                value,
                false);
}

void BluetoothAgent::RequestAuthorization(const QDBusObjectPath &device)
{
    if (!acceptsDevice(device)) {
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Unexpected Bluetooth device"));
        return;
    }
    queuePrompt(device,
                ReplyKind::Empty,
                QStringLiteral("authorization"),
                QStringLiteral("Allow Bluetooth device"),
                QStringLiteral("Allow %1 to pair with this computer?").arg(m_deviceName),
                {},
                false);
}

void BluetoothAgent::AuthorizeService(const QDBusObjectPath &device, const QString &uuid)
{
    Q_UNUSED(uuid)
    if (!acceptsDevice(device)) {
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Unexpected Bluetooth device"));
        return;
    }
    queuePrompt(device,
                ReplyKind::Empty,
                QStringLiteral("service"),
                QStringLiteral("Bluetooth service"),
                QStringLiteral("Allow %1 to connect?").arg(m_deviceName),
                {},
                false);
}

void BluetoothAgent::Cancel()
{
    clearPrompt();
    emit agentMessage(QStringLiteral("Bluetooth pairing was cancelled."));
}

bool BluetoothAgent::acceptsDevice(const QDBusObjectPath &device) const
{
    return !m_allowedDevicePath.isEmpty() && device.path() == m_allowedDevicePath;
}

void BluetoothAgent::queuePrompt(const QDBusObjectPath &device,
                                 ReplyKind replyKind,
                                 const QString &kind,
                                 const QString &title,
                                 const QString &messageText,
                                 const QString &value,
                                 bool needsInput)
{
    if (!acceptsDevice(device))
        return;
    setDelayedReply(true);
    m_pendingMessage = message();
    m_replyKind = replyKind;
    m_promptKind = kind;
    m_promptTitle = title;
    m_promptMessage = messageText;
    m_promptValue = value;
    m_promptNeedsInput = needsInput;
    m_promptVisible = true;
    emit promptChanged();
}

void BluetoothAgent::showInformation(const QDBusObjectPath &device,
                                     const QString &title,
                                     const QString &messageText,
                                     const QString &value)
{
    if (!acceptsDevice(device))
        return;
    m_replyKind = ReplyKind::None;
    m_promptKind = QStringLiteral("information");
    m_promptTitle = title;
    m_promptMessage = messageText;
    m_promptValue = value;
    m_promptNeedsInput = false;
    m_promptVisible = true;
    emit promptChanged();
}

void BluetoothAgent::clearPrompt()
{
    const bool changed = m_promptVisible || m_replyKind != ReplyKind::None;
    m_pendingMessage = {};
    m_replyKind = ReplyKind::None;
    m_promptKind.clear();
    m_promptTitle.clear();
    m_promptMessage.clear();
    m_promptValue.clear();
    m_promptVisible = false;
    m_promptNeedsInput = false;
    if (changed)
        emit promptChanged();
}
