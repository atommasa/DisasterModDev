// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGCoroutine.h"
#include "RPGFlowUnrealTypes.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct FRPGVoidCoroutine final
#if CPP
	: public TRPGCoroutine<>
#endif // CPP
{
	GENERATED_BODY()

	FRPGVoidCoroutine() noexcept
		: TRPGCoroutine<>()
	{}

	FRPGVoidCoroutine(const TRPGCoroutine<>& InCoroutine) noexcept
		: TRPGCoroutine<>(InCoroutine)
	{}

	FRPGVoidCoroutine(TRPGCoroutine<>&& InCoroutine) noexcept
		: TRPGCoroutine<>(MoveTemp(InCoroutine))
	{}
};

static_assert(sizeof(FRPGVoidCoroutine) == sizeof(TRPGCoroutine<>));