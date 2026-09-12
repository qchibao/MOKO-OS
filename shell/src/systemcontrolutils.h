#pragma once

#include <QList>
#include <QString>
#include <QVariantList>

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

// A Wi-Fi device is only considered fully connected when NetworkManager has
// supplied the basic data needed for actual network use.
bool networkConnectionReady(uint deviceState,
                            bool activeConnection,
                            bool hasAddress,
                            bool hasRoute,
                            bool hasDns);
QVariantList normalizeWifiNetworks(const QVariantList &networks,
                                   const QString &activeAccessPoint,
                                   bool connectionReady);

QList<AudioEndpoint> parseWpctlEndpoints(const QString &output, const QString &sectionName);
AudioLevel parseWpctlVolume(const QString &output);
QString decodeSsid(const QByteArray &bytes);
int percentage(qint64 value, qint64 maximum);

} // namespace MokoSystemControl
