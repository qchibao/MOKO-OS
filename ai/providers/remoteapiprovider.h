#pragma once

#include "aiprovider.h"

// Interface only: provider implementations must obtain credentials from the
// user environment or secret storage, never from source or the live image.
class RemoteApiProvider : public AiProvider
{
public:
    ~RemoteApiProvider() override = default;

    virtual bool isConfigured() const = 0;
    virtual QString configurationError() const = 0;
};
