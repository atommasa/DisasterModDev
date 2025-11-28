// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RPGPlayerController.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPC, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlledCharacterChanged);

UENUM(BlueprintType)
enum class ERPGControlMode : uint8
{
	None			UMETA(DisplayName = "None"),
	Gameplay		UMETA(DisplayName = "Gameplay"),
	UI				UMETA(DisplayName = "UI"),
	MIXED			UMETA(DisplayName = "Mixed"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlModeChanged, const ERPGControlMode, NewControlMode);

/**
 * The PlayerController class is the base class for Player Controllers in the IronicRPG plugin.
 */
UCLASS()
class RPGCORE_API ARPGPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ARPGPlayerController(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Controller")
	virtual void PossessCharacter(APawn* NewCharacter, ERPGControlMode ControlMode = ERPGControlMode::Gameplay);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category = "Controller")
	virtual void SetControlMode(ERPGControlMode NewControlMode);

public:
	// Called when the controlled character chenges, provides a delegate for other components to listen to
	FOnControlledCharacterChanged OnControlledCharacterChanged;

	// Called when the control mode changes
	FOnControlModeChanged OnControlModeChanged;
};
