#pragma once

#include <QObject>
#include <QVariantMap>

class WindowManager;

class DesktopSettingsService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.moko.Desktop1")
    Q_PROPERTY(QString appearanceMode READ appearanceMode NOTIFY appearanceChanged)
    Q_PROPERTY(bool safeGraphics READ safeGraphics CONSTANT)
    Q_PROPERTY(bool glassEffectsEnabled READ glassEffectsEnabled NOTIFY appearanceChanged)

public:
    explicit DesktopSettingsService(WindowManager *windowManager, QObject *parent = nullptr);

    QString appearanceMode() const;
    bool safeGraphics() const;
    bool glassEffectsEnabled() const;

public slots:
    QVariantMap inputState() const;
    QVariantMap appearanceState() const;
    bool setNaturalScrollEnabled(bool enabled);
    bool setPointerAcceleration(int speed);
    bool setThreeFingerDragEnabled(bool enabled);
    bool setBrowserHistorySwipeEnabled(bool enabled);
    bool setAppearanceMode(const QString &mode);

signals:
    void inputChanged();
    void appearanceChanged();

private:
    WindowManager *m_windowManager = nullptr;
    QString m_appearanceMode;
    bool m_safeGraphics = false;
};
