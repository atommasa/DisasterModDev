// Copyright Ironic Studio. All Rights Reserved.


#include "DataTypes/RPGId.h"
#include "RPGSettings.h"
#include "Assets/RPGAssetManager.h"

int32 FRPGId::GetNumeric() const
{
	const URPGSettings* RPGSettings = URPGSettings::GetRPGSettings();
	if (!RPGSettings)
	{
		return 0;
	}

	const int32 NumericLen = RPGSettings->NumericLen;
	const FString IdStr = Id.ToString();

	if (IdStr.Len() < NumericLen)
	{
		return 0; // Not enough characters for a numeric value
	}

	FString NumericPart = IdStr.Right(NumericLen);
	int32 NumericValue = FCString::Atoi(*NumericPart);

	return NumericValue;
}

FName FRPGId::GetRebuiltIdType() const
{
	const URPGSettings* RPGSettings = URPGSettings::GetRPGSettings();
	if (!RPGSettings)
	{
		return ID_None;
	}

	const int32 MaxPrefixLength = RPGSettings->MaxPrefixLen;
	const int32 NumericLen = RPGSettings->NumericLen;

	// Prefix at least one letter (NumericLen + 1) and can not be longer than MaxPrefixLength + NumericLen
	const FString IdStr = Id.ToString();
	if (IdStr.Len() < NumericLen + 1 || IdStr.Len() > MaxPrefixLength + NumericLen)
	{
		return ID_None;
	}

	const FRegexPattern Pattern(FString::Printf(TEXT("^[a-zA-Z]{1,%d}\\d{%d}$"), MaxPrefixLength, NumericLen));
	FRegexMatcher Matcher(Pattern, IdStr);

	if (!Matcher.FindNext())
	{
		return ID_None;
	}

	FName Prefix(IdStr.LeftChop(NumericLen));

	return URPGAssetManager::Get().GetIdTypeFromPrefix(Prefix);
}

FName FRPGId::GetIdType() const
{
	if (CachedId == Id && CachedType != ID_None)
	{
		return CachedType;
	}

	const FName Type = GetRebuiltIdType();
	CachedId = Id;
	CachedType = Type;
	return Type;
}

FString FRPGId::GetIdTypeString() const
{
	return GetIdType().ToString();
}
