// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FRPGFlowModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
