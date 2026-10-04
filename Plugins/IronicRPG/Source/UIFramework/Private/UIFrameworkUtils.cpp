// Copyright Ironic Studio. All Rights Reserved.


#include "UIFrameworkUtils.h"
#include "Components/UIControlComponent.h"

#include "Kismet/GameplayStatics.h"

UUIControlComponent* UUIFrameworkUtils::GetUIControlComponent(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
    if (!PC)
    {
        return nullptr;
    }

    return PC->GetComponentByClass<UUIControlComponent>();
}
