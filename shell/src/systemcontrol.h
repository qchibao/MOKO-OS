#pragma once

#include "bluetoothagent.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

class QTimer;

class SystemControl final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool networkManagerAvailable READ networkManagerAvailable NOTIFY networkChanged)
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY networkChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY networkChanged)
    Q_PROPERTY(bool wifiScanning READ wifiScanning NOTIFY networkChanged)
    Q_PROPERTY(bool networkBusy READ networkBusy NOTIFY networkChanged)
    Q_PROPERTY(QString wifiState READ wifiState NOTIFY networkChanged)
    Q_PROPERTY(QString activeSsid READ activeSsid NOTIFY networkChanged)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY networkChanged)

    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable NOTIFY bluetoothChanged)
    Q_PROPERTY(bool bluetoothPowered READ bluetoothPowered NOTIFY bluetoothChanged)
    Q_PROPERTY(bool bluetoothScanning READ bluetoothScanning NOTIFY bluetoothChanged)
    Q_PROPERTY(bool bluetoothBusy READ bluetoothBusy NOTIFY bluetoothChanged)
    Q_PROPERTY(QVariantList bluetoothDevices READ bluetoothDevices NOTIFY bluetoothChanged)
    Q_PROPERTY(bool bluetoothPromptVisible READ bluetoothPromptVisible NOTIFY bluetoothPromptChanged)
    Q_PROPERTY(QString bluetoothPromptKind READ bluetoothPromptKind NOTIFY bluetoothPromptChanged)
    Q_PROPERTY(QString bluetoothPromptTitle READ bluetoothPromptTitle NOTIFY bluetoothPromptChanged)
    Q_PROPERTY(QString bluetoothPromptMessage READ bluetoothPromptMessage NOTIFY bluetoothPromptChanged)
    Q_PROPERTY(QString bluetoothPromptValue READ bluetoothPromptValue NOTIFY bluetoothPromptChanged)
    Q_PROPERTY(bool bluetoothPromptNeedsInput READ bluetoothPromptNeedsInput NOTIFY bluetoothPromptChanged)

    Q_PROPERTY(bool audioAvailable READ audioAvailable NOTIFY audioChanged)
    Q_PROPERTY(int outputVolume READ outputVolume NOTIFY audioChanged)
    Q_PROPERTY(bool outputMuted READ outputMuted NOTIFY audioChanged)
    Q_PROPERTY(QString outputDeviceName READ outputDeviceName NOTIFY audioChanged)
    Q_PROPERTY(QVariantList outputDevices READ outputDevices NOTIFY audioChanged)
    Q_PROPERTY(int inputVolume READ inputVolume NOTIFY audioChanged)
    Q_PROPERTY(bool inputMuted READ inputMuted NOTIFY audioChanged)
    Q_PROPERTY(QString inputDeviceName READ inputDeviceName NOTIFY audioChanged)
    Q_PROPERTY(QVariantList inputDevices READ inputDevices NOTIFY audioChanged)

    Q_PROPERTY(bool brightnessAvailable READ brightnessAvailable NOTIFY powerChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY powerChanged)
    Q_PROPERTY(bool batteryAvailable READ batteryAvailable NOTIFY powerChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY powerChanged)
    Q_PROPERTY(int batteryHealth READ batteryHealth NOTIFY powerChanged)
    Q_PROPERTY(QString batteryState READ batteryState NOTIFY powerChanged)
    Q_PROPERTY(QString batteryTime READ batteryTime NOTIFY powerChanged)
    Q_PROPERTY(QString batteryTechnology READ batteryTechnology NOTIFY powerChanged)
    Q_PROPERTY(QString batteryCycleCount READ batteryCycleCount NOTIFY powerChanged)
    Q_PROPERTY(QString batteryEnergy READ batteryEnergy NOTIFY powerChanged)
    Q_PROPERTY(bool externalPowerConnected READ externalPowerConnected NOTIFY powerChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY powerChanged)
    Q_PROPERTY(bool powerModeAvailable READ powerModeAvailable NOTIFY powerChanged)
    Q_PROPERTY(QString powerMode READ powerMode NOTIFY powerChanged)
    Q_PROPERTY(QStringList powerModes READ powerModes NOTIFY powerChanged)
    Q_PROPERTY(bool suspendAvailable READ suspendAvailable NOTIFY powerChanged)
    Q_PROPERTY(bool suspendPending READ suspendPending NOTIFY powerChanged)

    Q_PROPERTY(QString clockText READ clockText NOTIFY timeChanged)
    Q_PROPERTY(QString dateText READ dateText NOTIFY timeChanged)
    Q_PROPERTY(QString timeZoneName READ timeZoneName NOTIFY timeChanged)

    Q_PROPERTY(QString operationMessage READ operationMessage NOTIFY operationMessageChanged)

public:
    // Settings uses deferred startup so a slow system D-Bus cannot block its first frame.
    explicit SystemControl(QObject *parent = nullptr, bool deferInitialRefresh = false);
    ~SystemControl() override;

    bool networkManagerAvailable() const;
    bool wifiAvailable() const;
    bool wifiEnabled() const;
    bool wifiScanning() const;
    bool networkBusy() const;
    QString wifiState() const;
    QString activeSsid() const;
    QVariantList wifiNetworks() const;

    bool bluetoothAvailable() const;
    bool bluezServiceAvailable() const;
    bool bluetoothPowered() const;
    bool bluetoothScanning() const;
    bool bluetoothBusy() const;
    QVariantList bluetoothDevices() const;
    bool bluetoothPromptVisible() const;
    QString bluetoothPromptKind() const;
    QString bluetoothPromptTitle() const;
    QString bluetoothPromptMessage() const;
    QString bluetoothPromptValue() const;
    bool bluetoothPromptNeedsInput() const;

    bool audioAvailable() const;
    int outputVolume() const;
    bool outputMuted() const;
    QString outputDeviceName() const;
    QVariantList outputDevices() const;
    int inputVolume() const;
    bool inputMuted() const;
    QString inputDeviceName() const;
    QVariantList inputDevices() const;

    bool brightnessAvailable() const;
    int brightness() const;
    bool batteryAvailable() const;
    int batteryPercent() const;
    int batteryHealth() const;
    QString batteryState() const;
    QString batteryTime() const;
    QString batteryTechnology() const;
    QString batteryCycleCount() const;
    QString batteryEnergy() const;
    bool externalPowerConnected() const;
    bool batteryCharging() const;
    bool powerModeAvailable() const;
    QString powerMode() const;
    QStringList powerModes() const;
    bool suspendAvailable() const;
    bool suspendPending() const;
    QString clockText() const;
    QString dateText() const;
    QString timeZoneName() const;
    QString operationMessage() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void preloadNetwork();
    Q_INVOKABLE void preloadBluetooth();
    Q_INVOKABLE bool setWifiEnabled(bool enabled);
    Q_INVOKABLE bool requestWifiScan();
    Q_INVOKABLE bool connectWifi(const QString &ssid, const QString &password);
    Q_INVOKABLE bool disconnectWifi();

    Q_INVOKABLE bool setBluetoothPowered(bool powered);
    Q_INVOKABLE bool setBluetoothScanning(bool scanning);
    Q_INVOKABLE bool pairBluetoothDevice(const QString &deviceId);
    Q_INVOKABLE bool connectBluetoothDevice(const QString &deviceId);
    Q_INVOKABLE bool disconnectBluetoothDevice(const QString &deviceId);
    Q_INVOKABLE bool forgetBluetoothDevice(const QString &deviceId);
    Q_INVOKABLE void answerBluetoothPrompt(const QString &value, bool accepted);

    Q_INVOKABLE bool setOutputVolume(int percent);
    Q_INVOKABLE bool setOutputMuted(bool muted);
    Q_INVOKABLE bool setOutputDevice(int id);
    Q_INVOKABLE bool setInputVolume(int percent);
    Q_INVOKABLE bool setInputMuted(bool muted);
    Q_INVOKABLE bool setInputDevice(int id);

    Q_INVOKABLE bool setBrightness(int percent);
    Q_INVOKABLE bool adjustBrightness(int delta);
    Q_INVOKABLE bool setPowerMode(const QString &mode);
    Q_INVOKABLE bool suspend();
    Q_INVOKABLE void reportControlCenterOpened(int page);

signals:
    void networkChanged();
    void bluetoothChanged();
    void bluetoothPromptChanged();
    void audioChanged();
    void powerChanged();
    void timeChanged();
    void operationMessageChanged();

private:
    void refreshNetwork();
    void refreshBluetooth();
    void refreshAudio();
    void refreshPower();
    void refreshTime();
    void ensureBluetoothAgent();
    bool runWpctl(const QStringList &arguments, QString *output = nullptr);
    bool bluetoothCall(const QString &deviceId,
                       const QString &method,
                       const QString &successMessage,
                       bool startsPairing = false);
    QVariantMap bluetoothDevice(const QString &deviceId) const;
    QVariantMap wifiNetwork(const QString &ssid) const;
    bool validAudioDevice(int id, const QVariantList &devices) const;
    void setOperationMessage(const QString &message);

    QTimer *m_refreshTimer = nullptr;
    bool m_targetedRefreshOnly = false;
    bool m_networkRefreshEnabled = false;
    bool m_bluetoothRefreshEnabled = false;
    bool m_networkPreloadPending = false;
    bool m_bluetoothPreloadPending = false;
    BluetoothAgent m_bluetoothAgent;
    QString m_bluetoothAgentPath;
    bool m_bluetoothAgentRegistered = false;

    bool m_networkManagerAvailable = false;
    bool m_wifiAvailable = false;
    bool m_wifiEnabled = false;
    bool m_wifiScanning = false;
    bool m_networkBusy = false;
    QString m_wifiState = QStringLiteral("Unavailable");
    QString m_activeSsid;
    QString m_wifiDevicePath;
    QVariantList m_wifiNetworks;

    bool m_bluetoothAvailable = false;
    bool m_bluezServiceAvailable = false;
    bool m_bluetoothPowered = false;
    bool m_bluetoothScanning = false;
    bool m_bluetoothBusy = false;
    QString m_bluetoothAdapterPath;
    QVariantList m_bluetoothDevices;

    bool m_audioAvailable = false;
    int m_outputVolume = 0;
    bool m_outputMuted = false;
    QString m_outputDeviceName;
    QVariantList m_outputDevices;
    int m_inputVolume = 0;
    bool m_inputMuted = false;
    QString m_inputDeviceName;
    QVariantList m_inputDevices;

    bool m_brightnessAvailable = false;
    int m_brightness = 0;
    QString m_backlightDevice;
    bool m_batteryAvailable = false;
    int m_batteryPercent = -1;
    int m_batteryHealth = -1;
    QString m_batteryState = QStringLiteral("Not detected");
    QString m_batteryTime;
    QString m_batteryTechnology;
    QString m_batteryCycleCount;
    QString m_batteryEnergy;
    bool m_externalPowerConnected = false;
    bool m_batteryCharging = false;
    bool m_powerModeAvailable = false;
    QString m_powerMode;
    QStringList m_powerModes;
    bool m_suspendAvailable = false;
    bool m_suspendPending = false;
    QString m_clockText;
    QString m_dateText;
    QString m_timeZoneName;
    QString m_operationMessage;
};
