#pragma once

#include <QQuickImageProvider>

class ApplicationIconProvider final : public QQuickImageProvider
{
public:
    ApplicationIconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
