// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "DataTypes/RPGId.h"
#include "RPGPlayerStart.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, ClassGroup = RPG, hidecategories = Collision)
class RPGCORE_API ARPGPlayerStart : public APlayerStart
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Object, meta=(IdType = "Zone"))
	FRPGId ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Object, meta = (IdType = "SubZone"))
	TArray<FRPGId> SubZoneIds;

};
