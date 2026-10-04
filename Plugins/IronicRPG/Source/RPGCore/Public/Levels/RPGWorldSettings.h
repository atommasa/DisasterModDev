// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "DataTypes/RPGId.h"
#include "Levels/GameZoneBinding.h"
#include "Levels/GameZoneContext.h"
#include "RPGWorldSettings.generated.h"

/**
 *
 */
UCLASS()
class RPGCORE_API ARPGWorldSettings : public AWorldSettings
{
    GENERATED_BODY()

#if WITH_EDITOR
    friend class FGameZoneBindingCoordinator;
#endif

public:
    UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
    FRPGId GetGameZoneId() const { return GameZoneId; }

    const FGuid& GetGameZoneBindingId() const { return GameZoneBindingId; }

    UFUNCTION(BlueprintCallable, Category = "RPG|GameZone")
    FGameZoneEntryId GetDefaultEntryId() const { return DefaultEntryId; }

#if WITH_EDITOR
    virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

protected:
    UPROPERTY(VisibleAnywhere, Category = "RPG")
    FRPGId GameZoneId;

    UPROPERTY(VisibleAnywhere, Category = "RPG")
    FGuid GameZoneBindingId;

    UPROPERTY(EditAnywhere, Category = "RPG", meta=(SourceWorld = "GameZoneId"))
    FGameZoneEntryId DefaultEntryId;

};
