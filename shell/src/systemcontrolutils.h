#pragma once

#include <QList>
#include <QString>

namespace MokoSystemControl {

struct AudioEndpoint
{
    int id = -1;
    QString name;
    bool defaultDevice = false;
};

struct AudioLevel
{
    int percent = 0;
    bool muted = false;
    bool valid = false;
};

QList<AudioEndpoint> parseWpctlEndpoints(const QString &output, const QString &sectionName);
AudioLevel parseWpctlVolume(const QString &output);
QString decodeSsid(const QByteArray &bytes);
int percentage(qint64 value, qint64 maximum);

} // namespace MokoSystemControl
