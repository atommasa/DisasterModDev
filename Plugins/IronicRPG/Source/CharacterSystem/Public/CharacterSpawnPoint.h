// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/SpawnablePoint.h"
#include "Characters/BaseCharacter.h"
#include "AIDataTypes.h"
#include "CharacterSpawnPoint.generated.h"

/**
 * A spawn point for characters in the game world.
 */
UCLASS(Blueprintable)
class CHARACTERSYSTEM_API ACharacterSpawnPoint : public ASpawnablePoint
{
	GENERATED_BODY()
	
public:	
	ACharacterSpawnPoint(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

#if WITH_EDITORONLY_DATA
protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void SpawnEditorPreviewMesh();

	virtual void OnAssetModified(const FRPGId& ModifiedAssetId);
#endif // WITH_EDITORONLY_DATA

protected:
	// The id of the character to spawn at this point.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn Point", meta=(IdType = "Character", DisplayPriority = 0))
	FRPGId CharacterId;

	// The class of the character to spawn at this point.
	// Can not spawn PlayableCharacter directly.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn Point", meta=(DisallowedClasses = "/Script/RPGCore.PlayableCharacter", DisplayPriority = 20))
	TSubclassOf<ABaseCharacter> SpawnClass = ABaseCharacter::StaticClass();

	// Patrol data to assign to the spawned character if applicable.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn Point", meta=(DisplayPriority = 100))
	FPatrolData PatrolData;

	// The character spawned at this point.
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Point")
	TWeakObjectPtr<ABaseCharacter> SpawnedCharacter;

public:
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	ABaseCharacter* GetSpawnedCharacter() const { return SpawnedCharacter.Get(); }

public: // ASpawnablePoint interface
	UFUNCTION(BlueprintCallable, Category = "Spawnable Point")
	virtual void Awaken() override;

};
