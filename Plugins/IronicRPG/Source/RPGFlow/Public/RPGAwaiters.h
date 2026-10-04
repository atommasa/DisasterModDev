// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGCoroutine.h"
#include "RPGObjectLifetimeRegistry.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "TimerManager.h"

#include "Delegates/Delegate.h"
#include "Templates/Tuple.h"
#include "Misc/Optional.h"

namespace RPGFlow
{
    class FNextTickAwaiter
    {
    public:
        explicit FNextTickAwaiter(UWorld* InWorld)
            : World(InWorld)
        {
        }

        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> InCoroutine)
        {
            check(IsInGameThread());
            Coroutine = InCoroutine;

            if (!World.IsValid())
            {
                Private::ResumeOnGameThread(Coroutine);
                return;
            }

            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateLambda([Coroutine = Coroutine]() mutable
                {
                    Private::ResumeOnGameThread(Coroutine);
                }));
        }

        void await_resume() const noexcept
        {
        }

    private:
        TWeakObjectPtr<UWorld> World;
        std::coroutine_handle<> Coroutine;
    };

    class FDelayAwaiter
    {
    public:
        FDelayAwaiter(UWorld* InWorld, float InSeconds)
            : World(InWorld), Seconds(FMath::Max(0.0f, InSeconds))
        {
        }

        bool await_ready() const noexcept
        {
            return Seconds <= 0.0f;
        }

        void await_suspend(std::coroutine_handle<> InCoroutine)
        {
            check(IsInGameThread());
            Coroutine = InCoroutine;

            UWorld* StrongWorld = World.Get();
            if (!StrongWorld)
            {
                Private::ResumeOnGameThread(Coroutine);
                return;
            }

            StrongWorld->GetTimerManager().SetTimer(
                TimerHandle,
                FTimerDelegate::CreateLambda([Coroutine = Coroutine]() mutable
                {
                    Private::ResumeOnGameThread(Coroutine);
                }),
                Seconds,
                false);
        }

        void await_resume() const noexcept
        {
        }

        ~FDelayAwaiter()
        {
            if (UWorld* StrongWorld = World.Get(); StrongWorld && TimerHandle.IsValid())
            {
                StrongWorld->GetTimerManager().ClearTimer(TimerHandle);
            }
        }

    private:
        TWeakObjectPtr<UWorld> World;
        float Seconds = 0.0f;
        FTimerHandle TimerHandle;
        std::coroutine_handle<> Coroutine;
    };

    class FStreamableHandleAwaiter
    {
    private:
        struct FResumeState
        {
            std::coroutine_handle<> Continuation{};
            bool bResumeRequested = false;
            bool bCanceled = false;

            void RequestResume()
            {
                check(IsInGameThread());

                if (bResumeRequested || bCanceled)
                {
                    return;
                }

                bResumeRequested = true;

                const std::coroutine_handle<> HandleToResume =
                    std::exchange(
                        Continuation,
                        std::coroutine_handle<>{});

                if (HandleToResume)
                {
                    RPGFlow::Private::ResumeOnGameThreadDeferred(
                        HandleToResume);
                }
            }

            void Cancel()
            {
                check(IsInGameThread());

                bCanceled = true;
                Continuation = {};
            }
        };

    public:
        explicit FStreamableHandleAwaiter(
            TSharedPtr<FStreamableHandle> InHandle)
            : Handle(MoveTemp(InHandle))
            , ResumeState(
                MakeShared<FResumeState, ESPMode::ThreadSafe>())
        {}

        FStreamableHandleAwaiter(const FStreamableHandleAwaiter&) = delete;

        FStreamableHandleAwaiter& operator=(const FStreamableHandleAwaiter&) = delete;

        bool await_ready() const noexcept
        {
            return IsFinished();
        }

        bool await_suspend(
            std::coroutine_handle<> InContinuation)
        {
            check(IsInGameThread());
            check(Handle.IsValid());

            ResumeState->Continuation = InContinuation;

            const TSharedRef<FResumeState, ESPMode::ThreadSafe>
                CapturedState = ResumeState;

            Handle->BindCompleteDelegate(
                FStreamableDelegate::CreateLambda(
                    [CapturedState]()
                    {
                        check(IsInGameThread());

                        UE_LOG(
                            LogTemp,
                            Warning,
                            TEXT(
                                "[RPGFlow] "
                                "Streamable complete delegate"));

                        CapturedState->RequestResume();
                    }));

            if (IsFinished())
            {
                ResumeState->RequestResume();
            }

            return true;
        }

        void await_resume() const noexcept
        {}

        ~FStreamableHandleAwaiter()
        {
            if (ResumeState->Continuation)
            {
                ResumeState->Cancel();
            }
        }

    private:
        bool IsFinished() const noexcept
        {
            return
                !Handle.IsValid() ||
                Handle->HasLoadCompleted() ||
                Handle->WasCanceled();
        }

    private:
        TSharedPtr<FStreamableHandle> Handle;

        TSharedRef<FResumeState, ESPMode::ThreadSafe> ResumeState;
    };

    enum class ERPGWaitStatus : uint8
    {
        Completed,
        Canceled,
        OwnerEndedPlay,
        OwnerDestroyed
    };

    template <typename TValue>
    struct TRPGWaitResult
    {
        ERPGWaitStatus Status = ERPGWaitStatus::Canceled;
        TOptional<TValue> Value;

        [[nodiscard]] bool WasCompleted() const
        {
            return Status == ERPGWaitStatus::Completed;
        }

        [[nodiscard]] bool WasCanceled() const
        {
            return Status == ERPGWaitStatus::Canceled;
        }

        [[nodiscard]] bool WasOwnerEndedPlay() const
        {
            return Status == ERPGWaitStatus::OwnerEndedPlay;
        }

        [[nodiscard]] bool WasOwnerDestroyed() const
        {
            return Status == ERPGWaitStatus::OwnerDestroyed;
        }

        [[nodiscard]] bool WasOwnerInvalidated() const
        {
            return WasOwnerEndedPlay() || WasOwnerDestroyed();
        }
    };

    template <typename TDelegate, typename... TArgs>
    class TDelegateAwaiter
    {
    public:
        using FValueType = TTuple<std::decay_t<TArgs>...>;
        using FResultType = TRPGWaitResult<FValueType>;

        static_assert(
            (std::is_constructible_v<std::decay_t<TArgs>, TArgs> && ...),
            "RPGFlow::WaitDelegate cannot store one or more delegate arguments.");

    private:
        struct FState
            : public TSharedFromThis<FState, ESPMode::ThreadSafe>
        {
            TDelegate* Delegate = nullptr;
            FDelegateHandle DelegateHandle;

            TWeakObjectPtr<UObject> Owner;
            const UObjectBase* OwnerKey = nullptr;
            Private::FObjectLifetimeRegistry::FRegistrationId
                OwnerRegistrationId = 0;
            bool bTrackOwner = false;
            bool bOwnerEndedPlay = false;
            bool bOwnerDeleted = false;

            TWeakPtr<Private::FCoroutineStateBase, ESPMode::ThreadSafe>
                CoroutineState;
            std::coroutine_handle<> Continuation{};

            FResultType Result;
            bool bCompleted = false;

            void HandleBroadcast(TArgs... Args)
            {
                check(IsInGameThread());

                if (bCompleted)
                {
                    return;
                }

                bCompleted = true;
                Result.Status = ERPGWaitStatus::Completed;
                Result.Value.Emplace(Forward<TArgs>(Args)...);

                FinishAndResume();
            }


            void HandleOwnerLifetimeEnded(
                ERPGObjectLifetimeEndReason Reason)
            {
                check(IsInGameThread());

                if (bCompleted)
                {
                    return;
                }

                bCompleted = true;
                Result.Value.Reset();

                // The registry removes the registration before invoking us.
                OwnerRegistrationId = 0;
                OwnerKey = nullptr;

                if (Reason == ERPGObjectLifetimeEndReason::EndPlay)
                {
                    bOwnerEndedPlay = true;
                    Result.Status = ERPGWaitStatus::OwnerEndedPlay;

                    // EndPlay is an early notification. The UObject and its
                    // delegate storage are still valid, so unbind normally.
                    if (Delegate && DelegateHandle.IsValid())
                    {
                        Delegate->Remove(DelegateHandle);
                    }

                    DelegateHandle.Reset();
                    Delegate = nullptr;
                    Owner.Reset();
                }
                else
                {
                    bOwnerDeleted = true;
                    Result.Status = ERPGWaitStatus::OwnerDestroyed;

                    // The UObject has already left GUObjectArray. Never touch
                    // delegate storage that belonged to it.
                    Owner.Reset();
                    Delegate = nullptr;
                    DelegateHandle.Reset();
                }

                UnregisterCancellation();

                const std::coroutine_handle<> HandleToResume =
                    std::exchange(
                        Continuation,
                        std::coroutine_handle<>{});

                if (HandleToResume)
                {
                    Private::ResumeOnGameThreadDeferred(HandleToResume);
                }
            }

            void HandleCancellation(ERPGCoroutineCancelReason)
            {
                check(IsInGameThread());

                if (bCompleted)
                {
                    return;
                }

                bCompleted = true;
                Result.Status = ERPGWaitStatus::Canceled;
                Result.Value.Reset();

                FinishAndResume();
            }

            void CancelWithoutResume()
            {
                check(IsInGameThread());

                if (bCompleted)
                {
                    return;
                }

                bCompleted = true;
                Result.Status = ERPGWaitStatus::Canceled;
                Result.Value.Reset();
                Continuation = {};

                UnregisterCancellation();
                UnregisterOwnerLifetime();
                Unbind();
            }

            void FinishAndResume()
            {
                UnregisterCancellation();
                UnregisterOwnerLifetime();
                Unbind();

                const std::coroutine_handle<> HandleToResume =
                    std::exchange(
                        Continuation,
                        std::coroutine_handle<>{});

                if (HandleToResume)
                {
                    Private::ResumeOnGameThreadDeferred(HandleToResume);
                }
            }

            void UnregisterCancellation()
            {
                if (const TSharedPtr<Private::FCoroutineStateBase, ESPMode::ThreadSafe>
                    StrongState = CoroutineState.Pin())
                {
                    StrongState->UnregisterCancellationHandler(this);
                }

                CoroutineState.Reset();
            }

            void UnregisterOwnerLifetime()
            {
                if (OwnerRegistrationId == 0 || !OwnerKey)
                {
                    return;
                }

                Private::FObjectLifetimeRegistry::Get().Unregister(
                    OwnerKey,
                    OwnerRegistrationId);

                OwnerRegistrationId = 0;
                OwnerKey = nullptr;
            }

            void Unbind()
            {
                if (!DelegateHandle.IsValid())
                {
                    return;
                }

                // When an explicit UObject owner is tracked and already invalid,
                // the delegate storage may no longer be safe to dereference.
                if (bTrackOwner &&
                    (bOwnerDeleted || bOwnerEndedPlay || !Owner.IsValid()))
                {
                    DelegateHandle.Reset();
                    Delegate = nullptr;
                    return;
                }

                if (Delegate)
                {
                    Delegate->Remove(DelegateHandle);
                }

                DelegateHandle.Reset();
            }
        };

    public:
        explicit TDelegateAwaiter(TDelegate& InDelegate)
            : State(MakeShared<FState, ESPMode::ThreadSafe>())
        {
            State->Delegate = &InDelegate;
        }

        TDelegateAwaiter(UObject* InOwner, TDelegate& InDelegate)
            : State(MakeShared<FState, ESPMode::ThreadSafe>())
        {
            State->Owner = InOwner;
            State->bTrackOwner = true;
            State->Delegate = &InDelegate;
        }

        TDelegateAwaiter(const TDelegateAwaiter&) = delete;
        TDelegateAwaiter& operator=(const TDelegateAwaiter&) = delete;

        TDelegateAwaiter(TDelegateAwaiter&&) noexcept = default;
        TDelegateAwaiter& operator=(TDelegateAwaiter&&) noexcept = default;

        bool await_ready() const noexcept
        {
            if (!State->bTrackOwner)
            {
                return false;
            }

            UObject* StrongOwner = State->Owner.Get();
            if (!StrongOwner)
            {
                State->bCompleted = true;
                State->Result.Status = ERPGWaitStatus::OwnerDestroyed;
                return true;
            }

            if (Private::FObjectLifetimeRegistry::Get()
                .IsObjectEndingPlay(StrongOwner))
            {
                State->bCompleted = true;
                State->bOwnerEndedPlay = true;
                State->Result.Status = ERPGWaitStatus::OwnerEndedPlay;
                return true;
            }

            return false;
        }

        template <typename TPromise>
        bool await_suspend(std::coroutine_handle<TPromise> InCoroutine)
        {
            check(IsInGameThread());
            check(State->Delegate);
            check(!State->Continuation);
            check(!State->DelegateHandle.IsValid());

            if (State->bTrackOwner)
            {
                UObject* StrongOwner = State->Owner.Get();
                if (!StrongOwner)
                {
                    State->bCompleted = true;
                    State->Result.Status = ERPGWaitStatus::OwnerDestroyed;
                    return false;
                }

                if (Private::FObjectLifetimeRegistry::Get()
                    .IsObjectEndingPlay(StrongOwner))
                {
                    State->bCompleted = true;
                    State->bOwnerEndedPlay = true;
                    State->Result.Status = ERPGWaitStatus::OwnerEndedPlay;
                    return false;
                }
            }

            State->Continuation = InCoroutine;

            const TSharedRef<FState, ESPMode::ThreadSafe> CapturedState =
                State.ToSharedRef();

            if (State->bTrackOwner)
            {
                UObject* StrongOwner = State->Owner.Get();
                if (!StrongOwner)
                {
                    State->bCompleted = true;
                    State->Result.Status = ERPGWaitStatus::OwnerDestroyed;
                    State->Continuation = {};
                    return false;
                }

                State->OwnerKey = StrongOwner;
                const TWeakPtr<FState, ESPMode::ThreadSafe> WeakState = State;

                State->OwnerRegistrationId =
                    Private::FObjectLifetimeRegistry::Get().Register(
                        State->OwnerKey,
                        [WeakState](ERPGObjectLifetimeEndReason Reason)
                        {
                            if (const TSharedPtr<FState, ESPMode::ThreadSafe>
                                AwaiterState = WeakState.Pin())
                            {
                                AwaiterState->HandleOwnerLifetimeEnded(Reason);
                            }
                        });

                if (State->OwnerRegistrationId == 0)
                {
                    State->bCompleted = true;
                    State->bOwnerEndedPlay = true;
                    State->Result.Status = ERPGWaitStatus::OwnerEndedPlay;
                    State->OwnerKey = nullptr;
                    State->Continuation = {};
                    return false;
                }
            }

            State->DelegateHandle =
                State->Delegate->AddLambda(
                    [CapturedState](TArgs... Args)
                    {
                        CapturedState->HandleBroadcast(
                            Forward<TArgs>(Args)...);
                    });

            State->CoroutineState = InCoroutine.promise().GetState();

            if (const TSharedPtr<Private::FCoroutineStateBase, ESPMode::ThreadSafe>
                StrongState = State->CoroutineState.Pin())
            {
                const TWeakPtr<FState, ESPMode::ThreadSafe> WeakState = State;

                StrongState->RegisterCancellationHandler(
                    State.Get(),
                    [WeakState](ERPGCoroutineCancelReason Reason)
                    {
                        if (const TSharedPtr<FState, ESPMode::ThreadSafe>
                            AwaiterState = WeakState.Pin())
                        {
                            AwaiterState->HandleCancellation(Reason);
                        }
                    });
            }

            // Close the registration window. These operations are game-thread
            // only, but this also handles teardown initiated by callbacks during
            // setup.
            if (State->bTrackOwner)
            {
                UObject* StrongOwner = State->Owner.Get();
                if (!StrongOwner)
                {
                    State->HandleOwnerLifetimeEnded(
                        ERPGObjectLifetimeEndReason::Destroyed);
                }
                else if (Private::FObjectLifetimeRegistry::Get()
                    .IsObjectEndingPlay(StrongOwner))
                {
                    State->HandleOwnerLifetimeEnded(
                        ERPGObjectLifetimeEndReason::EndPlay);
                }
            }

            return true;
        }

        FResultType await_resume()
        {
            check(State->bCompleted);
            return MoveTemp(State->Result);
        }

        ~TDelegateAwaiter()
        {
            if (!State.IsValid() || State->bCompleted)
            {
                return;
            }

            if (IsInGameThread())
            {
                State->CancelWithoutResume();
            }
            else
            {
                const TSharedPtr<FState, ESPMode::ThreadSafe> CapturedState = State;

                AsyncTask(
                    ENamedThreads::GameThread,
                    [CapturedState]()
                    {
                        if (CapturedState.IsValid())
                        {
                            CapturedState->CancelWithoutResume();
                        }
                    });
            }
        }

    private:
        TSharedPtr<FState, ESPMode::ThreadSafe> State;
    };

    inline FNextTickAwaiter NextTick(UWorld* World)
    {
        return FNextTickAwaiter(World);
    }

    inline FDelayAwaiter Delay(UWorld* World, float Seconds)
    {
        return FDelayAwaiter(World, Seconds);
    }

    inline FStreamableHandleAwaiter Streamable(TSharedPtr<FStreamableHandle> Handle)
    {
        return FStreamableHandleAwaiter(MoveTemp(Handle));
    }

    namespace Private
    {
        /**
         * Extracts the argument list from an Unreal delegate's Broadcast member
         * function and constructs the matching TDelegateAwaiter.
         *
         * Unreal's delegate aliases do not expose their signature through a
         * uniform public typedef, but Broadcast always carries the delegate's
         * invocation parameters.
         */
        template <typename TMemberFunction>
        struct TDelegateBroadcastTraits;

#define RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(Qualifiers)                    \
        template <typename TObject, typename TReturn, typename... TArgs>         \
        struct TDelegateBroadcastTraits<                                        \
            TReturn (TObject::*)(TArgs...) Qualifiers>                          \
        {                                                                        \
            template <typename TDelegate>                                        \
            static auto Make(TDelegate& Delegate)                                \
            {                                                                    \
                return TDelegateAwaiter<TDelegate, TArgs...>(Delegate);           \
            }                                                                    \
                                                                                 \
            template <typename TDelegate>                                        \
            static auto Make(UObject* Owner, TDelegate& Delegate)                \
            {                                                                    \
                return TDelegateAwaiter<TDelegate, TArgs...>(Owner, Delegate);    \
            }                                                                    \
        };

        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS()
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(volatile)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const volatile)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(volatile noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const volatile noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(&)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const &)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(volatile &)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const volatile &)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(& noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const & noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(volatile & noexcept)
        RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS(const volatile & noexcept)

#undef RPGFLOW_DEFINE_DELEGATE_BROADCAST_TRAITS
    }

    /**
     * Waits for a native Unreal multicast delegate and automatically infers its
     * broadcast parameters. Example: void(ABaseCharacter*) becomes
     * TDelegateAwaiter<TDelegate, ABaseCharacter*>.
     */
    template <typename TDelegate>
    auto WaitDelegate(TDelegate& Delegate)
    {
        using FDelegateType = std::remove_cv_t<
            std::remove_reference_t<TDelegate>>;
        using FBroadcastFunction = decltype(&FDelegateType::Broadcast);
        using FTraits = Private::TDelegateBroadcastTraits<FBroadcastFunction>;

        return FTraits::Make(Delegate);
    }

    template <typename TDelegate>
    auto WaitDelegate(UObject* Owner, TDelegate& Delegate)
    {
        using FDelegateType = std::remove_cv_t<
            std::remove_reference_t<TDelegate>>;
        using FBroadcastFunction = decltype(&FDelegateType::Broadcast);
        using FTraits = Private::TDelegateBroadcastTraits<FBroadcastFunction>;

        return FTraits::Make(Owner, Delegate);
    }

    /**
     * Explicit fallback for unusual delegate-like types whose Broadcast member
     * is overloaded or otherwise cannot be inspected with decltype.
     */
    template <typename... TArgs, typename TDelegate>
    auto WaitDelegateExplicit(TDelegate& Delegate)
    {
        return TDelegateAwaiter<TDelegate, TArgs...>(Delegate);
    }

    template <typename... TArgs, typename TDelegate>
    auto WaitDelegateExplicit(UObject* Owner, TDelegate& Delegate)
    {
        return TDelegateAwaiter<TDelegate, TArgs...>(Owner, Delegate);
    }
}
