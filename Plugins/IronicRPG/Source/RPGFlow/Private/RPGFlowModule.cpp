#include "RPGFlowModule.h"

#include "Modules/ModuleManager.h"
#include "RPGObjectLifetimeRegistry.h"

IMPLEMENT_MODULE(FRPGFlowModule, RPGFlow)

void FRPGFlowModule::StartupModule()
{
    RPGFlow::Private::FObjectLifetimeRegistry::Get().Initialize();
}

void FRPGFlowModule::ShutdownModule()
{
    RPGFlow::Private::FObjectLifetimeRegistry::Get().Shutdown();
}
