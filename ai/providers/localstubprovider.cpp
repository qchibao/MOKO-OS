#include "localstubprovider.h"

#include <QRegularExpression>

namespace {

AiIntent action(const QString &name,
                const QVariantMap &parameters,
                const QString &response)
{
    return {name, parameters, response, true};
}

bool containsAny(const QString &text, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (text.contains(needle))
            return true;
    }
    return false;
}

} // namespace

QString LocalStubProvider::name() const
{
    return QStringLiteral("local-stub");
}

AiIntent LocalStubProvider::interpret(const QString &prompt) const
{
    const QString normalized = prompt.simplified().toLower();
    if (normalized.isEmpty())
        return {};

    if (containsAny(normalized,
                    {QStringLiteral("shell"), QStringLiteral("sudo"),
                     QStringLiteral("command"), QStringLiteral("rm -rf")})) {
        return action(QStringLiteral("refuse"), {},
                      QStringLiteral("MOKO AI cannot execute shell commands."));
    }

    if (containsAny(normalized,
                    {QStringLiteral("open"), QStringLiteral("launch"),
                     QStringLiteral("start"), QStringLiteral("show")})) {
        if (containsAny(normalized, {QStringLiteral("files"), QStringLiteral("file manager")}))
            return action(QStringLiteral("open_application"),
                          {{QStringLiteral("appId"), QStringLiteral("org.moko.Files")}},
                          QStringLiteral("Opening MOKO Files."));
        if (normalized.contains(QStringLiteral("settings")))
            return action(QStringLiteral("open_application"),
                          {{QStringLiteral("appId"), QStringLiteral("org.moko.Settings")}},
                          QStringLiteral("Opening MOKO Settings."));
        if (normalized.contains(QStringLiteral("terminal")))
            return action(QStringLiteral("open_application"),
                          {{QStringLiteral("appId"), QStringLiteral("org.moko.Terminal")}},
                          QStringLiteral("Opening MOKO Terminal."));
        if (containsAny(normalized, {QStringLiteral("hardware diagnostics"),
                                     QStringLiteral("diagnostics")}))
            return action(QStringLiteral("open_application"),
                          {{QStringLiteral("appId"), QStringLiteral("org.moko.HardwareDiagnostics")}},
                          QStringLiteral("Opening MOKO Hardware Diagnostics."));
    }

    if (containsAny(normalized,
                    {QStringLiteral("system overview"), QStringLiteral("system summary"),
                     QStringLiteral("system information"), QStringLiteral("system info"),
                     QStringLiteral("about this system"), QStringLiteral("computer info")})) {
        return action(QStringLiteral("system_summary"), {},
                      QStringLiteral("Here is the current system summary."));
    }
    if (normalized.contains(QStringLiteral("battery")))
        return action(QStringLiteral("battery_status"), {},
                      QStringLiteral("Here is the current battery status."));
    if (containsAny(normalized,
                    {QStringLiteral("network"), QStringLiteral("wifi"),
                     QStringLiteral("wi-fi")})) {
        return action(QStringLiteral("network_status"), {},
                      QStringLiteral("Here is the current network status."));
    }
    if (containsAny(normalized,
                    {QStringLiteral("storage"), QStringLiteral("disk space"),
                     QStringLiteral("free space")})) {
        return action(QStringLiteral("storage_status"), {},
                      QStringLiteral("Here is the current storage status."));
    }

    const QRegularExpression searchExpression(
        QStringLiteral("^(?:find|search(?: files?)?)\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch searchMatch = searchExpression.match(prompt.simplified());
    if (searchMatch.hasMatch()) {
        return action(QStringLiteral("search_files"),
                      {{QStringLiteral("query"), searchMatch.captured(1).trimmed()}},
                      QStringLiteral("Searching files in your home directory."));
    }

    return action(QStringLiteral("unsupported"), {},
                  QStringLiteral("I can open MOKO apps, search your home files, or report system, battery, network and storage status."));
}
