#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

inline void writeMokoLiveEvent(const QString &event)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty() || event.contains(u'\n') || event.contains(u'\r'))
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(event.toUtf8());
    file.write("\n");
    file.flush();
}
