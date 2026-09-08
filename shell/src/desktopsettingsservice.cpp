#include "desktopsettingsservice.h"

#include "windowmanager.h"

DesktopSettingsService::DesktopSettingsService(WindowManager *windowManager, QObject *parent)
    : QObject(parent)
    , m_windowManager(windowManager)
{
    connect(m_windowManager, &WindowManager::inputChanged,
            this, &DesktopSettingsService::inputChanged);
    connect(m_windowManager, &WindowManager::connectedChanged,
            this, &DesktopSettingsService::inputChanged);
}

QVariantMap DesktopSettingsService::inputState() const
{
    return {
        {QStringLiteral("connected"), m_windowManager->connected()},
        {QStringLiteral("touchpadAvailable"), m_windowManager->touchpadAvailable()},
        {QStringLiteral("touchpadCount"), m_windowManager->touchpadCount()},
        {QStringLiteral("tapToClickEnabled"), m_windowManager->tapToClickEnabled()},
        {QStringLiteral("twoFingerScrollEnabled"), m_windowManager->twoFingerScrollEnabled()},
        {QStringLiteral("naturalScrollAvailable"), m_windowManager->naturalScrollAvailable()},
        {QStringLiteral("naturalScrollEnabled"), m_windowManager->naturalScrollEnabled()},
        {QStringLiteral("secondaryClickEnabled"), m_windowManager->secondaryClickEnabled()},
        {QStringLiteral("pointerAccelerationAvailable"),
         m_windowManager->pointerAccelerationAvailable()},
        {QStringLiteral("pointerAcceleration"), m_windowManager->pointerAcceleration()},
        {QStringLiteral("palmRejectionManaged"), m_windowManager->palmRejectionManaged()},
        {QStringLiteral("dragEnabled"), m_windowManager->dragEnabled()},
        {QStringLiteral("threeFingerDragAvailable"),
         m_windowManager->threeFingerDragAvailable()},
        {QStringLiteral("threeFingerDragEnabled"),
         m_windowManager->threeFingerDragEnabled()},
        {QStringLiteral("browserHistorySwipeAvailable"),
         m_windowManager->browserHistorySwipeAvailable()},
        {QStringLiteral("browserHistorySwipeEnabled"),
         m_windowManager->browserHistorySwipeEnabled()},
    };
}

bool DesktopSettingsService::setNaturalScrollEnabled(bool enabled)
{
    return m_windowManager->setNaturalScrollEnabled(enabled);
}

bool DesktopSettingsService::setPointerAcceleration(int speed)
{
    return m_windowManager->setPointerAcceleration(speed);
}

bool DesktopSettingsService::setThreeFingerDragEnabled(bool enabled)
{
    return m_windowManager->setThreeFingerDragEnabled(enabled);
}

bool DesktopSettingsService::setBrowserHistorySwipeEnabled(bool enabled)
{
    return m_windowManager->setBrowserHistorySwipeEnabled(enabled);
}
