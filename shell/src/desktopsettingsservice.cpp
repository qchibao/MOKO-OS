#include "desktopsettingsservice.h"

#include "windowmanager.h"

#include <QSettings>

namespace {

QString normalizedAppearanceMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("light") || normalized == QStringLiteral("dark")
        || normalized == QStringLiteral("glass")) {
        return normalized;
    }
    return QStringLiteral("light");
}

} // namespace

DesktopSettingsService::DesktopSettingsService(WindowManager *windowManager, QObject *parent)
    : QObject(parent)
    , m_windowManager(windowManager)
    , m_appearanceMode(normalizedAppearanceMode(
          QSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"))
              .value(QStringLiteral("appearance/mode"), QStringLiteral("light")).toString()))
    , m_safeGraphics(qEnvironmentVariableIntValue("MOKO_SAFE_GRAPHICS") == 1)
{
    connect(m_windowManager, &WindowManager::inputChanged,
            this, &DesktopSettingsService::inputChanged);
    connect(m_windowManager, &WindowManager::connectedChanged,
            this, &DesktopSettingsService::inputChanged);
}

QString DesktopSettingsService::appearanceMode() const
{
    return m_appearanceMode;
}

bool DesktopSettingsService::safeGraphics() const
{
    return m_safeGraphics;
}

bool DesktopSettingsService::glassEffectsEnabled() const
{
    return m_appearanceMode == QStringLiteral("glass") && !m_safeGraphics;
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

QVariantMap DesktopSettingsService::appearanceState() const
{
    return {
        {QStringLiteral("mode"), m_appearanceMode},
        {QStringLiteral("safeGraphics"), m_safeGraphics},
        {QStringLiteral("glassEffectsEnabled"), glassEffectsEnabled()},
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

bool DesktopSettingsService::setAppearanceMode(const QString &mode)
{
    const QString normalized = normalizedAppearanceMode(mode);
    if (normalized != mode.trimmed().toLower())
        return false;
    if (m_appearanceMode == normalized)
        return true;
    m_appearanceMode = normalized;
    QSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"))
        .setValue(QStringLiteral("appearance/mode"), normalized);
    emit appearanceChanged();
    return true;
}
