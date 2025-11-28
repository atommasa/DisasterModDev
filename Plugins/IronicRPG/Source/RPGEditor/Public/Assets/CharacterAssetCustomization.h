// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/RPGPrimaryAssetCustomization.h"

#include "Characters/CharacterAsset.h"
#include "Characters/Profiles/EnemyProfile.h"

#include "SEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"

class SCapsuleEditorViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SCapsuleEditorViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));

		SEditorViewport::Construct(SEditorViewport::FArguments());
	}

	void SetPreviewMesh(USkeletalMesh* InMesh)
	{
		if (!PreviewSkeletalMeshComp)
		{
			PreviewSkeletalMeshComp = NewObject<USkeletalMeshComponent>();
			PreviewScene->AddComponent(PreviewSkeletalMeshComp, FTransform::Identity);
		}

		PreviewSkeletalMeshComp->SetSkeletalMesh(InMesh);
		PreviewSkeletalMeshComp->SetRelativeLocation(FVector::ZeroVector);
		PreviewSkeletalMeshComp->SetRelativeRotation(FRotator::ZeroRotator);
	}

	void SetCapsuleSize(float HalfHeight, float Radius)
	{
		if (!PreviewCapsule)
		{
			PreviewCapsule = NewObject<UCapsuleComponent>();
			PreviewCapsule->SetCapsuleHalfHeight(HalfHeight);
			PreviewCapsule->SetCapsuleRadius(Radius);
			PreviewScene->AddComponent(PreviewCapsule, FTransform::Identity);
		}
		
		PreviewCapsule->SetCapsuleHalfHeight(HalfHeight);
		PreviewCapsule->SetCapsuleRadius(Radius);
		PreviewCapsule->SetWorldLocation(FVector(0.f, 0.f, HalfHeight));
	}

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
	{
		ViewportClient = MakeShareable(new FEditorViewportClient(nullptr, PreviewScene.Get()));
		ViewportClient->SetViewMode(VMI_Lit);
		ViewportClient->SetRealtime(true);
		ViewportClient->SetViewRotation(FRotator(-10.f, -90.f, 0.f));
		ViewportClient->SetViewLocationForOrbiting(FVector(0.f, 0.f, 88.f));
		ViewportClient->SetCameraSpeedSetting(4.0f);
		return ViewportClient.ToSharedRef();
	}

	virtual TSharedPtr<SWidget> MakeViewportToolbar() override { return SNullWidget::NullWidget; }

private:
	TSharedPtr<FEditorViewportClient> ViewportClient;
	TSharedPtr<class FAdvancedPreviewScene> PreviewScene;

	USkeletalMeshComponent* PreviewSkeletalMeshComp = nullptr;
	UCapsuleComponent* PreviewCapsule = nullptr;
};

/**
 *
 */
class FCharacterAssetCustomization : public FRPGPrimaryAssetCustomization<UCharacterAsset>
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShareable(new FCharacterAssetCustomization()); }

protected:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		FRPGPrimaryAssetCustomization<UCharacterAsset>::CustomizeDetails(DetailBuilder);

		IDetailCategoryBuilder& CapsuleCategory = DetailBuilder.EditCategory("Capsule");

		CapsuleCategory.AddCustomRow(FText::FromString(TEXT("Capsule Viewer")))
			.ValueContent()
			[
				SAssignNew(CapsuleViewport, SCapsuleEditorViewport)
			];

		USkeletalMesh* Mesh = CurrtentObject->GetDefaultData().CharacterMesh;
		CapsuleViewport->SetPreviewMesh(Mesh);
		CapsuleViewport->SetCapsuleSize(CurrtentObject->GetDefaultCapsuleHalfHeight(), CurrtentObject->GetDefaultCapsuleRadius());

		DetailBuilder.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& Categories)
			{
				const TMap<FName, int32> CustomOrder = {
					{"RPGPrimaryAsset", 1}, // General Category
					{"Character", 2},
					{"Profile", 3},
					{"Capsule", 4},
					{"AI", 5},
				};
				
				for (auto& Pair : Categories)
				{
					const int32* ForcedOrder = CustomOrder.Find(Pair.Key);
					if (ForcedOrder)
					{
						Pair.Value->SetSortOrder(*ForcedOrder);
					}
					else
					{
						// push others to bottom
						Pair.Value->SetSortOrder(Pair.Value->GetSortOrder() + 100);
					}
				}
			});
	}

	virtual void RefreshViewportFromAsset() override
	{
		if (!CapsuleViewport)
		{
			return;
		}

		CapsuleViewport->SetCapsuleSize(
			CurrtentObject->GetDefaultCapsuleHalfHeight(),
			CurrtentObject->GetDefaultCapsuleRadius()
		);

		USkeletalMesh* Mesh = CurrtentObject->GetDefaultData().CharacterMesh;
		CapsuleViewport->SetPreviewMesh(Mesh);
	}

private:
	TSharedPtr<SCapsuleEditorViewport> CapsuleViewport;

};
