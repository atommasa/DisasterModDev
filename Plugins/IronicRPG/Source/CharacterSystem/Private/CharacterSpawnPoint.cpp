// Copyright Ironic Studio. All Rights Reserved.


#include "CharacterSpawnPoint.h"
#include "CharacterSubsystem.h"

#include "Assets/RPGAssetLibrary.h"
#include "Characters/CharacterAsset.h"

#include "Components/CapsuleComponent.h"
#include "Components/BillboardComponent.h"

ACharacterSpawnPoint::ACharacterSpawnPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (SpawnIcon)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> IconTexture(TEXT("/Engine/EditorResources/S_NavP"));
		if (IconTexture.Succeeded())
		{
			SpawnIcon->SetSprite(IconTexture.Get());
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void ACharacterSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	
#if WITH_EDITORONLY_DATA
	// Hide preview mesh during gameplay
	if (PreviewMesh)
	{
		PreviewMesh->SetVisibility(false);
		PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	if (PreviewCapsule)
	{
		PreviewCapsule->SetVisibility(false);
		PreviewCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (SpawnIcon)
	{
		SpawnIcon->SetVisibility(false);
	}
#endif // WITH_EDITORONLY_DATA

}

#if WITH_EDITORONLY_DATA
void ACharacterSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SpawnEditorPreviewMesh();
}

void ACharacterSpawnPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName& PropertyName = PropertyChangedEvent.MemberProperty->GetFName();

	// Handle changes to CharacterId
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACharacterSpawnPoint, CharacterId))
	{
		SpawnEditorPreviewMesh();
	}
	// Make sure SpawnClass is not None
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACharacterSpawnPoint, SpawnClass))
	{
		if (SpawnClass == nullptr)
		{
			SpawnClass = ABaseCharacter::StaticClass();
		}
	}
}

void ACharacterSpawnPoint::SpawnEditorPreviewMesh()
{
	if (!PreviewMesh || !PreviewCapsule)
	{
		return;
	}

    if (CharacterId.IsValid())
    {
		URPGAssetLibrary::GetAssetByRPGIdAsync(CharacterId, {}, [WeakThis = TWeakObjectPtr<ACharacterSpawnPoint>(this)](URPGPrimaryAsset* Asset)
			{
				if (!WeakThis.IsValid() || !Asset)
				{
					return;
				}

				ACharacterSpawnPoint* This = WeakThis.Get();

				if (UCharacterAsset* CharacterAsset = Cast<UCharacterAsset>(Asset))
				{
					if (!CharacterAsset->OnRPGAssetModified.IsBoundToObject(This))
					{
						CharacterAsset->OnRPGAssetModified.AddUObject(This, &ACharacterSpawnPoint::OnAssetModified);
					}

					if (USkeletalMesh* Mesh = CharacterAsset->GetDefaultData().CharacterMesh)
					{
						This->PreviewMesh->SetSkeletalMesh(Mesh);
						This->PreviewMesh->SetVisibility(true);

						This->PreviewCapsule->SetCapsuleSize(
							CharacterAsset->GetDefaultCapsuleRadius(),
							CharacterAsset->GetDefaultCapsuleHalfHeight()
						);

						This->PreviewCapsule->SetWorldLocation(
							This->GetActorLocation() + FVector(0.f, 0.f, CharacterAsset->GetDefaultCapsuleHalfHeight())
						);

						This->PreviewCapsule->SetVisibility(true);

						return;
					}
				}

				This->PreviewMesh->SetVisibility(false);
				This->PreviewCapsule->SetVisibility(false);
			});
    }
	else
	{
		PreviewMesh->SetVisibility(false);
		PreviewCapsule->SetVisibility(false);
	}
}
void ACharacterSpawnPoint::OnAssetModified(const FRPGId& ModifiedAssetId)
{
	if (CharacterId == ModifiedAssetId)
	{
		SpawnEditorPreviewMesh();
	}
}
#endif // WITH_EDITORONLY_DATA

void ACharacterSpawnPoint::Awaken()
{
	if (!bSpawnOnLevelLoaded)
	{
		return;
	}

	UCharacterSubsystem* CharacterSubsystem = GetGameInstance()->GetSubsystem<UCharacterSubsystem>();
	if (CharacterSubsystem)
	{
		const FVector& SpawnLocation = GetActorLocation();
		const FRotator& SpawnRotation = GetActorRotation();

		SpawnedCharacter = CharacterSubsystem->SpawnCharacter(
			CharacterId,
			SpawnClass,
			SpawnLocation,
			SpawnRotation,
			false,
			SpawnGuid
		);

		if (SpawnedCharacter.IsValid())
		{
		}
	}
}
