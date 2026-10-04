// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RPGPlayerController.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPC, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnControlledCharacterChanged,
	const ABaseCharacter*, NewCharacter,
	const ABaseCharacter*, OldCharacter);

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERPGControlMode : uint8
{
	None        = 0x00			UMETA(Hidden),
	Gameplay	= 0x01			UMETA(DisplayName = "Gameplay"),
	UI			= 0x02			UMETA(DisplayName = "UI"),
	Mixed		= 0x03			UMETA(DisplayName = "Mixed"), // Gameplay || UI
};
ENUM_CLASS_FLAGS(ERPGControlMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlModeChanged, const int32&, NewControlMode);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, Category = "RPG|Controller")
	virtual void PossessCharacter(APawn* NewCharacter);

	UFUNCTION(BlueprintCallable, Category = "RPG|Controller")
	virtual void PossessCharacterWithMode(APawn* NewCharacter, ERPGControlMode ControlMode);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category = "RPG|Controller")
	ERPGControlMode GetControlMode() const { return CurrentControlMode; }

	UFUNCTION(BlueprintCallable, Category = "RPG|Controller")
	void SetControlMode(ERPGControlMode NewControlMode);

public:
	// Called when the controlled character chenges, provides a delegate for other components to listen to
	UPROPERTY(BlueprintAssignable, Category = "RPG|Controller")
	FOnControlledCharacterChanged OnControlledCharacterChanged;

	// Called when the control mode changes
	UPROPERTY(BlueprintAssignable, Category = "RPG|Controller")
	FOnControlModeChanged OnControlModeChanged;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, DisplayName = "Default Control Mode")
	ERPGControlMode CurrentControlMode;

};
