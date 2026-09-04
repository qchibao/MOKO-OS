#pragma once

#include <QString>
#include <QVariantMap>

struct AiIntent
{
    QString action;
    QVariantMap parameters;
    QString response;
    bool valid = false;
};

class AiProvider
{
public:
    virtual ~AiProvider() = default;

    virtual QString name() const = 0;
    virtual AiIntent interpret(const QString &prompt) const = 0;
};
