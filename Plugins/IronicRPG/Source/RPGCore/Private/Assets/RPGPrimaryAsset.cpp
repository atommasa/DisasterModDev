// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/RPGPrimaryAsset.h"
#include "UObject/AssetRegistryTagsContext.h"

void URPGPrimaryAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	Context.AddTag(FAssetRegistryTag(
		TEXT("RPGId"),
		Id.ToString(),
		UObject::FAssetRegistryTag::TT_Alphabetical
	));
}

#if WITH_EDITOR
void URPGPrimaryAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnRPGAssetModified.Broadcast(Id);
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void URPGPrimaryAsset::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		if (!Property->HasMetaData(TEXT("AssetBundles")))
		{
			continue;
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(this);

			AddStructSoftObjectToBundle(
				*Property->GetMetaData(TEXT("AssetBundles")),
				StructPtr,
				StructProp->Struct
			);
		}
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(this));
			FProperty* ElementProp = ArrayProp->Inner;

			if (FStructProperty* ElementStruct = CastField<FStructProperty>(ElementProp))
			{
				for (int32 i = 0; i < ArrayHelper.Num(); ++i)
				{
					void* ElementPtr = ArrayHelper.GetRawPtr(i);

					AddStructSoftObjectToBundle(
						*Property->GetMetaData(TEXT("AssetBundles")),
						ElementPtr,
						ElementStruct->Struct
					);
				}
			}
		}
		else if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper SetHelper(SetProp, SetProp->ContainerPtrToValuePtr<void>(this));
			FProperty* ElementProp = SetProp->ElementProp;

			if (FStructProperty* ElementStruct = CastField<FStructProperty>(ElementProp))
			{
				for (int32 i = 0; i < SetHelper.GetMaxIndex(); ++i)
				{
					void* ElementPtr = SetHelper.GetElementPtr(i);

					AddStructSoftObjectToBundle(
						*Property->GetMetaData(TEXT("AssetBundles")),
						ElementPtr,
						ElementStruct->Struct
					);
				}
			}
		}
		else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(this));
			FProperty* KeyProp = MapProp->KeyProp;
			FProperty* ValueProp = MapProp->ValueProp;

			// Only process struct keys
			if (FStructProperty* KeyStruct = CastField<FStructProperty>(KeyProp))
			{
				for (int32 i = 0; i < MapHelper.GetMaxIndex(); ++i)
				{
					void* KeyPtr = MapHelper.GetKeyPtr(i);

					AddStructSoftObjectToBundle(
						*Property->GetMetaData(TEXT("AssetBundles")),
						KeyPtr,
						KeyStruct->Struct
					);
				}
			}

			// Only process struct values
			if (FStructProperty* ValueStruct = CastField<FStructProperty>(ValueProp))
			{
				for (int32 i = 0; i < MapHelper.GetMaxIndex(); ++i)
				{
					void* ValuePtr = MapHelper.GetValuePtr(i);

					AddStructSoftObjectToBundle(
						*Property->GetMetaData(TEXT("AssetBundles")),
						ValuePtr,
						ValueStruct->Struct
					);
				}
			}
		}
	}
}

void URPGPrimaryAsset::AddStructSoftObjectToBundle(FName BundleName, const void* StructPtr, const UStruct* StructType)
{
	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* InnerProp = *It;

		if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(InnerProp))
		{
			FSoftObjectPtr SoftObj = SoftProp->GetPropertyValue_InContainer(StructPtr);

			if (!SoftObj.IsNull())
			{
				AssetBundleData.AddBundleAsset(BundleName, SoftObj.ToSoftObjectPath());
			}
		}
	}
}
#endif // WITH_EDITORONLY_DATA