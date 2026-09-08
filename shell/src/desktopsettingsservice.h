#pragma once

#include <QObject>
#include <QVariantMap>

class WindowManager;

class DesktopSettingsService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.moko.Desktop1")

public:
    explicit DesktopSettingsService(WindowManager *windowManager, QObject *parent = nullptr);

public slots:
    QVariantMap inputState() const;
    bool setNaturalScrollEnabled(bool enabled);
    bool setPointerAcceleration(int speed);
    bool setThreeFingerDragEnabled(bool enabled);
    bool setBrowserHistorySwipeEnabled(bool enabled);

signals:
    void inputChanged();

private:
    WindowManager *m_windowManager = nullptr;
};
