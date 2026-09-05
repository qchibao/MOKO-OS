#include "systemcontrolutils.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace MokoSystemControl {

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
