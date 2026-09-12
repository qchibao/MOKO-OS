#include "systemcontrolutils.h"

#include <QRegularExpression>
#include <QMap>

#include <algorithm>
#include <cmath>

namespace MokoSystemControl {

bool networkConnectionReady(uint deviceState,
                            bool activeConnection,
                            bool hasAddress,
                            bool hasRoute,
                            bool hasDns)
{
    return deviceState == 100 && activeConnection && hasAddress && hasRoute && hasDns;
}

QVariantList normalizeWifiNetworks(const QVariantList &networks,
                                   const QString &activeAccessPoint,
                                   bool connectionReady)
{
    QMap<QString, QVariantMap> strongestBySsid;
    for (const QVariant &networkValue : networks) {
        QVariantMap network = networkValue.toMap();
        const QString ssid = network.value(QStringLiteral("ssid")).toString().trimmed();
        if (ssid.isEmpty())
            continue;
        const bool active = !activeAccessPoint.isEmpty()
            && network.value(QStringLiteral("id")).toString() == activeAccessPoint;
        network.insert(QStringLiteral("active"), active);
        network.insert(QStringLiteral("connected"), active && connectionReady);
        const QVariantMap existing = strongestBySsid.value(ssid);
        if (existing.isEmpty() || active
            || (!existing.value(QStringLiteral("active")).toBool()
                && existing.value(QStringLiteral("strength")).toInt()
                    < network.value(QStringLiteral("strength")).toInt())) {
            strongestBySsid.insert(ssid, network);
        }
    }

    QVariantList result;
    for (const QVariantMap &network : strongestBySsid.values())
        result.append(network);
    std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();
        if (a.value(QStringLiteral("active")).toBool()
            != b.value(QStringLiteral("active")).toBool())
            return a.value(QStringLiteral("active")).toBool();
        return a.value(QStringLiteral("strength")).toInt()
            > b.value(QStringLiteral("strength")).toInt();
    });
    return result;
}

QList<AudioEndpoint> parseWpctlEndpoints(const QString &output, const QString &sectionName)
{
    QList<AudioEndpoint> endpoints;
    bool inSection = false;
    const QRegularExpression entryExpression(
        QStringLiteral("(\\*)?\\s*(\\d+)\\.\\s+(.+?)(?:\\s+\\[vol:.*)?$"));

    const QStringList lines = output.split(u'\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.endsWith(u':')) {
            inSection = trimmed.contains(sectionName, Qt::CaseInsensitive);
            continue;
        }
        if (!inSection)
            continue;

        const QRegularExpressionMatch match = entryExpression.match(line);
        if (!match.hasMatch())
            continue;
        bool idOk = false;
        const int id = match.captured(2).toInt(&idOk);
        QString name = match.captured(3).trimmed();
        if (!idOk || id < 0 || name.isEmpty())
            continue;
        const qsizetype properties = name.indexOf(QStringLiteral(" ["));
        if (properties > 0)
            name.truncate(properties);
        endpoints.append({id, name.trimmed(), line.contains(u'*')});
    }
    return endpoints;
}

AudioLevel parseWpctlVolume(const QString &output)
{
    const QRegularExpression expression(
        QStringLiteral("Volume:\\s*([0-9]+(?:\\.[0-9]+)?)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(output);
    if (!match.hasMatch())
        return {};

    bool ok = false;
    const double value = match.captured(1).toDouble(&ok);
    if (!ok)
        return {};
    return {std::clamp(static_cast<int>(std::lround(value * 100.0)), 0, 150),
            output.contains(QStringLiteral("MUTED"), Qt::CaseInsensitive),
            true};
}

QString decodeSsid(const QByteArray &bytes)
{
    QString result = QString::fromUtf8(bytes).trimmed();
    if (result.contains(QChar::ReplacementCharacter))
        result = QString::fromLatin1(bytes).trimmed();
    return result;
}

int percentage(qint64 value, qint64 maximum)
{
    if (value < 0 || maximum <= 0)
        return -1;
    return std::clamp(static_cast<int>(std::lround(
                          static_cast<double>(value) * 100.0 / static_cast<double>(maximum))),
                      0,
                      100);
}

} // namespace MokoSystemControl
