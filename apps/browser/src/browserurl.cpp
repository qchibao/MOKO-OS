#include "browserurl.h"

#include <QRegularExpression>
#include <QUrlQuery>

QUrl normalizedBrowserUrl(const QString &input)
{
    const QString value = input.trimmed();
    if (value.isEmpty())
        return QUrl(QStringLiteral("https://duckduckgo.com/"));

    const QUrl explicitUrl(value);
    const QString explicitScheme = explicitUrl.scheme().toLower();
    if ((explicitScheme == QStringLiteral("http")
         || explicitScheme == QStringLiteral("https"))
        && explicitUrl.isValid() && !explicitUrl.host().isEmpty()) {
        return explicitUrl;
    }

    if (!value.contains(QRegularExpression(QStringLiteral("\\s")))
        && (value.contains(u'.') || value.startsWith(QStringLiteral("localhost")))) {
        const QUrl hostUrl(QStringLiteral("https://") + value);
        if (hostUrl.isValid() && !hostUrl.host().isEmpty())
            return hostUrl;
    }

    QUrl searchUrl(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), value);
    searchUrl.setQuery(query);
    return searchUrl;
}
