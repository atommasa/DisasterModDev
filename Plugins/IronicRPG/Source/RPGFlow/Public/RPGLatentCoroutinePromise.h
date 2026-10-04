// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "RPGCoroutine.h"
#include "RPGLatent.h"
#include "RPGFlowUnrealTypes.h"

#include <coroutine>
#include <type_traits>

namespace RPGFlow::Private
{
	/** Promise used by FRPGVoidCoroutine UFUNCTION latent coroutines. */
	class FRPGLatentCoroutinePromise : public TRPGCoroutinePromise<void>
	{
	public:
		FRPGLatentCoroutinePromise() = default;

		template <typename TFirst, typename... TRest>
		FRPGLatentCoroutinePromise(TFirst&& First, TRest&&... Rest)
		{
			CaptureWorldContext(Forward<TFirst>(First));
			(CaptureLatentInfo(Forward<TRest>(Rest)), ...);
		}

		FRPGVoidCoroutine get_return_object()
		{
			TRPGCoroutine<> Coroutine(GetState());

			if (!bHasLatentInfo)
			{
				return FRPGVoidCoroutine(MoveTemp(Coroutine));
			}

			UObject* ContextObject = WorldContext.Get();

			if (!IsValid(ContextObject))
			{
				ContextObject = LatentInfo.CallbackTarget;
			}

			if (!IsValid(ContextObject) || !IsValid(LatentInfo.CallbackTarget))
			{
				return FRPGVoidCoroutine(MoveTemp(Coroutine));
			}

			const bool bRegistered =
				RPGFlow::RegisterLatentCoroutine(
				ContextObject,
				LatentInfo,
				Coroutine);

			return FRPGVoidCoroutine(MoveTemp(Coroutine));
		}

	private:
		template <typename T>
		void CaptureWorldContext(T&& Arg)
		{
			using FArgType = std::remove_cvref_t<T>;

			if constexpr (
				std::is_pointer_v<FArgType> &&
				TIsDerivedFrom<
				std::remove_pointer_t<FArgType>,
				UObject
				>::Value)
			{
				WorldContext = Arg;
			}
		}

		template <typename T>
		void CaptureLatentInfo(T&& Arg)
		{
			using FArgType = std::remove_cvref_t<T>;

			if constexpr (std::is_same_v<FArgType, FLatentActionInfo>)
			{
				LatentInfo = Arg;
				bHasLatentInfo = true;

				if (!WorldContext.IsValid() && IsValid(LatentInfo.CallbackTarget))
				{
					WorldContext = LatentInfo.CallbackTarget;
				}
			}
		}

		TWeakObjectPtr<UObject> WorldContext;
		FLatentActionInfo LatentInfo;
		bool bHasLatentInfo = false;
	};
}

/** Makes FRPGVoidCoroutine a valid C++20 coroutine return type. */
template <typename... TArgs>
struct std::coroutine_traits<FRPGVoidCoroutine, TArgs...>
{
	using promise_type = RPGFlow::Private::FRPGLatentCoroutinePromise;
};
