#pragma once

#include "aiprovider.h"

class LocalStubProvider final : public AiProvider
{
public:
    QString name() const override;
    AiIntent interpret(const QString &prompt) const override;
};
