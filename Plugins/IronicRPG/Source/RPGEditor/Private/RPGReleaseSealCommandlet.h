// Copyright Ironic Studio. All Rights Reserved.
#pragma once
#include "Commandlets/Commandlet.h"
#include "RPGReleaseSealCommandlet.generated.h"

UCLASS()
class URPGReleaseSealCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	URPGReleaseSealCommandlet();
	virtual int32 Main(const FString& Params) override;
};
