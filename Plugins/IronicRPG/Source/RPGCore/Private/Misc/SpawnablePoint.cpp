// Copyright Ironic Studio. All Rights Reserved.


#include "Misc/SpawnablePoint.h"

#include "Components/CapsuleComponent.h"
#include "Components/BillboardComponent.h"

ASpawnablePoint::ASpawnablePoint(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TObjectPtr<USceneComponent> SceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Scene Comp"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Movable;

	PreviewMesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Preview Mesh"));
	if (PreviewMesh)
	{
		PreviewMesh->SetupAttachment(RootComponent);
		PreviewMesh->SetRelativeRotation(FRotator(0.f, 270.f, 0.f));

		PreviewMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
		PreviewMesh->SetMobility(EComponentMobility::Movable);
		PreviewMesh->CastShadow = true;
		PreviewMesh->bCastDynamicShadow = true;
		PreviewMesh->bAffectDynamicIndirectLighting = true;
		PreviewMesh->SetReceivesDecals(true);
	}

	PreviewCapsule = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Preview Capsule"));
	if (PreviewCapsule)
	{
		PreviewCapsule->SetupAttachment(RootComponent);
		PreviewCapsule->SetHiddenInGame(true);
		PreviewCapsule->SetVisibility(false);
	}

	SpawnIcon = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Spawn Icon"));
	if (SpawnIcon)
	{
		SpawnIcon->SetupAttachment(RootComponent);
		SpawnIcon->bIsEditorOnly = true;
		SpawnIcon->bHiddenInGame = true;
		SpawnIcon->SetUsingAbsoluteScale(true);
		SpawnIcon->bIsScreenSizeScaled = true;
		SpawnIcon->SetVisibility(true);
	}
#endif // WITH_EDITORONLY_DATA
}
