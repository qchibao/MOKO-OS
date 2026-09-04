#include "applicationiconprovider.h"

#include <QFileInfo>
#include <QIcon>
#include <QUrl>

ApplicationIconProvider::ApplicationIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap ApplicationIconProvider::requestPixmap(const QString &id,
                                                QSize *size,
                                                const QSize &requestedSize)
{
    const QString iconName = QUrl::fromPercentEncoding(id.toUtf8());
    const QIcon icon = QFileInfo(iconName).isAbsolute() ? QIcon(iconName) : QIcon::fromTheme(iconName);
    if (icon.isNull()) {
        if (size)
            *size = {};
        return {};
    }

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(64, 64);
    targetSize = targetSize.expandedTo(QSize(16, 16));
    const QPixmap pixmap = icon.pixmap(targetSize);
    if (size)
        *size = pixmap.size();
    return pixmap;
}
