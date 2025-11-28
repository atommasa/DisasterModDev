// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/CharacterAsset.h"
#include "Characters/Attributes/LevelAttributeSet.h"

FCharacterSaveData UCharacterAsset::GetAttributesAtLevel(int32 Level) const
{
    FCharacterSaveData ResultData = DefaultData;
    if (GrowthTable.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("GrowthTable is not set in CharacterAsset for %s"), *GetName());
        return ResultData;
    }

    UCurveTable* Table = GrowthTable.LoadSynchronous();
    if (!Table)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load GrowthTable in CharacterAsset for %s"), *GetName());
        return ResultData;
    }

    for (auto& Pair : ResultData.Attributes)
    {
        FName RowName(Pair.Key.AttributeName);
        if (FRealCurve* Curve = Table->FindCurve(RowName, ""))
        {
            Pair.Value = Pair.Value * Curve->Eval(Level);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Row %s not found in GrowthTable for CharacterAsset %s"),
                *RowName.ToString(), *GetName());
        }
    }

    return ResultData;
}

FCharacterSaveData UCharacterAsset::GetDefaultLevelAttributes() const
{
    const float* DefaultLevel = DefaultData.Attributes.Find(ULevelAttributeSet::GetLevelAttribute());
    
    return GetAttributesAtLevel(DefaultLevel ? *DefaultLevel : 1.0f);
}

template<typename T>
T* UCharacterAsset::GetCharacterProfile() const
{
    for (const FInstancedStruct& ProfileStruct : Profiles)
    {
        if (ProfileStruct.GetScriptStruct()->IsChildOf(T::StaticStruct()))
        {
            return ProfileStruct.GetMutablePtr<T>();
        }
    }

    return nullptr;
}

#if WITH_EDITOR
void UCharacterAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterAsset, DefaultCapsuleHalfHeight))
    {
        DefaultCapsuleHalfHeight = FMath::Max3(0.f, DefaultCapsuleHalfHeight, DefaultCapsuleRadius);
    }
    else if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterAsset, DefaultCapsuleRadius))
    {
        DefaultCapsuleRadius = FMath::Clamp(DefaultCapsuleRadius, 0.f, DefaultCapsuleHalfHeight);
    }
}

void UCharacterAsset::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedChainEvent);

    const FName PropertyName = PropertyChangedChainEvent.Property ? PropertyChangedChainEvent.Property->GetFName() : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(UCharacterAsset, Profiles))
    {
        const uint32 CurrentIndex = PropertyChangedChainEvent.GetArrayIndex(PropertyChangedChainEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetName());
        
        if (Profiles.IsValidIndex(CurrentIndex))
        {
            const UScriptStruct* Struct = Profiles[CurrentIndex].GetScriptStruct();
            for (int32 i = 0; i < Profiles.Num(); i++)
            {
                if (i == CurrentIndex)
                {
                    continue;
                }

                if (Profiles[i].GetScriptStruct() == Struct)
                {
                    Profiles[i].Reset();
                }
            }
        }
    }
}
#endif // WITH_EDITOR
