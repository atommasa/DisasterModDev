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
	}

	~FRPGPrimaryAssetCustomization()
	{
		FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
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
