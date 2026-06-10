// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"

#include "Assets/RPGPrimaryAsset.h"

template <typename T>
concept AssetType = std::is_base_of_v<URPGPrimaryAsset, T>;

/**
 * 
 */
template <AssetType T>
class FRPGPrimaryAssetCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShareable(new FRPGPrimaryAssetCustomization()); }

	~FRPGPrimaryAssetCustomization()
	{
		FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
	}

protected:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> SelectObjects = DetailBuilder.GetSelectedObjects();
		for (TWeakObjectPtr<UObject>& Object : SelectObjects)
		{
			if (Object.IsValid() && Object->GetClass()->IsChildOf(T::StaticClass()))
			{
				CurrtentObject = Cast<T>(Object);
			}
		}

		check(CurrtentObject.IsValid());

		FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FRPGPrimaryAssetCustomization::OnObjectTransacted);

		IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory(URPGPrimaryAsset::StaticClass()->GetFName(), FText::FromString(TEXT("General")));

		DetailBuilder.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& Categories)
			{
				const TMap<FName, int32> CustomOrder = {
					{"RPGPrimaryAsset", 1}, // General Category
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

	virtual void RefreshViewportFromAsset() {}

private:
	// Call when the current object is modified
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
	{
		if (CurrtentObject == Object)
		{
			RefreshViewportFromAsset();
		}
	}

protected:
	TWeakObjectPtr<T> CurrtentObject;

};
