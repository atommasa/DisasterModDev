// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"

#include <coroutine>
#include <type_traits>
#include <utility>

namespace RPGFlow
{
    enum class ERPGCoroutineCancelReason : uint8
    {
        None,
        Requested,
        LatentObjectDestroyed,
        LatentActionAborted
    };
}

namespace RPGFlow::Private
{
    enum class ECoroutineStatus : uint8
    {
        Running,
        Succeeded,
        Failed
    };

    inline void ResumeOnGameThread(std::coroutine_handle<> Handle)
    {
        if (!Handle)
        {
            return;
        }

        if (IsInGameThread())
        {
            Handle.resume();
            return;
        }

        AsyncTask(ENamedThreads::GameThread, [Handle]() mutable
        {
            if (Handle)
            {
                Handle.resume();
            }
        });
    }

    inline void DestroyOnGameThread(std::coroutine_handle<> Handle)
    {
        if (!Handle)
        {
            return;
        }

        AsyncTask(ENamedThreads::GameThread, [Handle]() mutable
        {
            if (Handle)
            {
                Handle.destroy();
            }
        });
    }

    inline void ResumeOnGameThreadDeferred(
        std::coroutine_handle<> Handle)
    {
        if (!Handle)
        {
            return;
        }

        AsyncTask(
            ENamedThreads::GameThread,
            [Handle]() mutable
            {
                if (Handle)
                {
                    Handle.resume();
                }
            });
    }

    class FCoroutineStateBase : public TSharedFromThis<FCoroutineStateBase, ESPMode::ThreadSafe>
    {
    public:
        virtual ~FCoroutineStateBase() = default;

        [[nodiscard]] bool IsDone() const
        {
            return Status != ECoroutineStatus::Running;
        }

        [[nodiscard]] bool WasSuccessful() const
        {
            return Status == ECoroutineStatus::Succeeded;
        }

        [[nodiscard]] bool IsCancellationRequested() const
        {
            return bCancellationRequested;
        }

        [[nodiscard]] RPGFlow::ERPGCoroutineCancelReason GetCancelReason() const
        {
            return CancelReason;
        }

        void RequestCancel(RPGFlow::ERPGCoroutineCancelReason InReason)
        {
            check(IsInGameThread());

            if (IsDone() || bCancellationRequested)
            {
                return;
            }

            bCancellationRequested = true;
            CancelReason = InReason;

            if (CancellationHandler)
            {
                // The handler may unregister itself while it runs.
                const TFunction<void(RPGFlow::ERPGCoroutineCancelReason)>
                    HandlerCopy = CancellationHandler;
                HandlerCopy(InReason);
            }
        }

        void RegisterCancellationHandler(
            const void* Owner,
            TFunction<void(RPGFlow::ERPGCoroutineCancelReason)> Handler)
        {
            check(IsInGameThread());
            check(Owner);
            check(!CancellationHandlerOwner || CancellationHandlerOwner == Owner);

            CancellationHandlerOwner = Owner;
            CancellationHandler = MoveTemp(Handler);

            if (bCancellationRequested && CancellationHandler)
            {
                // The handler may unregister itself while it runs.
                const TFunction<void(RPGFlow::ERPGCoroutineCancelReason)>
                    HandlerCopy = CancellationHandler;
                HandlerCopy(CancelReason);
            }
        }

        void UnregisterCancellationHandler(const void* Owner)
        {
            check(IsInGameThread());

            if (CancellationHandlerOwner == Owner)
            {
                CancellationHandlerOwner = nullptr;
                CancellationHandler = nullptr;
            }
        }

        void AddContinuation(std::coroutine_handle<> Continuation)
        {
            check(IsInGameThread());

            if (IsDone())
            {
                ResumeOnGameThreadDeferred(Continuation);
                return;
            }

            Continuations.Add(Continuation);
        }

        void MarkSucceeded()
        {
            check(IsInGameThread());
            check(Status == ECoroutineStatus::Running);
            Status = ECoroutineStatus::Succeeded;
        }

        void MarkFailed()
        {
            check(IsInGameThread());
            check(Status == ECoroutineStatus::Running);
            Status = ECoroutineStatus::Failed;
        }

        void NotifyFinalSuspend(std::coroutine_handle<> CoroutineHandle)
        {
            check(IsInGameThread());

            TArray<std::coroutine_handle<>> PendingContinuations = MoveTemp(Continuations);
            Continuations.Reset();

            for (std::coroutine_handle<> Continuation : PendingContinuations)
            {
                ResumeOnGameThreadDeferred(Continuation);
            }

            // The result/state lives outside the coroutine frame. Destroying is
            // deferred so final_suspend can finish before the frame is released.
            DestroyOnGameThread(CoroutineHandle);
        }

    private:
        ECoroutineStatus Status = ECoroutineStatus::Running;
        TArray<std::coroutine_handle<>> Continuations;

        bool bCancellationRequested = false;
        RPGFlow::ERPGCoroutineCancelReason CancelReason =
            RPGFlow::ERPGCoroutineCancelReason::None;
        const void* CancellationHandlerOwner = nullptr;
        TFunction<void(RPGFlow::ERPGCoroutineCancelReason)> CancellationHandler;
    };

    template <typename TResult>
    class TCoroutineState final : public FCoroutineStateBase
    {
    public:
        template <typename TValue>
        void SetResult(TValue&& InResult)
        {
            Result.Emplace(Forward<TValue>(InResult));
        }

        [[nodiscard]] const TResult& GetResult() const
        {
            check(Result.IsSet());
            return Result.GetValue();
        }

    private:
        TOptional<TResult> Result;
    };

    template <>
    class TCoroutineState<void> final : public FCoroutineStateBase
    {
    };

    struct FFinalSuspendAwaiter
    {
        TSharedRef<FCoroutineStateBase, ESPMode::ThreadSafe> State;

        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> Handle) const noexcept
        {
            State->NotifyFinalSuspend(Handle);
        }

        void await_resume() const noexcept
        {
        }
    };
}

template <typename TResult = void>
class TRPGCoroutine;

template <typename TResult>
class TRPGCoroutinePromise
{
public:
    using FState = RPGFlow::Private::TCoroutineState<TResult>;

    TRPGCoroutinePromise()
        : State(MakeShared<FState, ESPMode::ThreadSafe>())
    {
        check(IsInGameThread());
    }

    TRPGCoroutine<TResult> get_return_object();

    std::suspend_never initial_suspend() const noexcept
    {
        return {};
    }

    RPGFlow::Private::FFinalSuspendAwaiter final_suspend() const noexcept
    {
        return { State };
    }

    template <typename TValue>
    void return_value(TValue&& Value)
    {
        State->SetResult(Forward<TValue>(Value));
        State->MarkSucceeded();
    }

    void unhandled_exception()
    {
        State->MarkFailed();
    }

    TSharedRef<FState, ESPMode::ThreadSafe> GetState() const
    {
        return State;
    }

private:
    TSharedRef<FState, ESPMode::ThreadSafe> State;
};

template <>
class TRPGCoroutinePromise<void>
{
public:
    using FState = RPGFlow::Private::TCoroutineState<void>;

    TRPGCoroutinePromise()
        : State(MakeShared<FState, ESPMode::ThreadSafe>())
    {
        check(IsInGameThread());
    }

    TRPGCoroutine<void> get_return_object();

    std::suspend_never initial_suspend() const noexcept
    {
        return {};
    }

    RPGFlow::Private::FFinalSuspendAwaiter final_suspend() const noexcept
    {
        return { State };
    }

    void return_void()
    {
        State->MarkSucceeded();
    }

    void unhandled_exception()
    {
        State->MarkFailed();
    }

    TSharedRef<FState, ESPMode::ThreadSafe> GetState() const
    {
        return State;
    }

private:
    TSharedRef<FState, ESPMode::ThreadSafe> State;
};

template <typename TResult>
class TRPGCoroutineAwaiter
{
public:
    using FState = RPGFlow::Private::TCoroutineState<TResult>;

    explicit TRPGCoroutineAwaiter(TSharedPtr<FState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    bool await_ready() const noexcept
    {
        return !State.IsValid() || State->IsDone();
    }

    template <typename TPromise>
    void await_suspend(std::coroutine_handle<TPromise> Continuation)
    {
        check(IsInGameThread());

        ParentState = Continuation.promise().GetState();
        const TWeakPtr<FState, ESPMode::ThreadSafe> WeakChild = State;

        ParentState.Pin()->RegisterCancellationHandler(
            this,
            [WeakChild](RPGFlow::ERPGCoroutineCancelReason Reason)
            {
                if (const TSharedPtr<FState, ESPMode::ThreadSafe> Child = WeakChild.Pin())
                {
                    Child->RequestCancel(Reason);
                }
            });

        State->AddContinuation(Continuation);
    }

    TResult await_resume()
    {
        UnregisterParentCancellation();
        check(State.IsValid());
        check(State->WasSuccessful());
        return State->GetResult();
    }

    ~TRPGCoroutineAwaiter()
    {
        UnregisterParentCancellation();
    }

private:
    void UnregisterParentCancellation()
    {
        if (const TSharedPtr<RPGFlow::Private::FCoroutineStateBase, ESPMode::ThreadSafe> Parent = ParentState.Pin())
        {
            Parent->UnregisterCancellationHandler(this);
        }
        ParentState.Reset();
    }

    TSharedPtr<FState, ESPMode::ThreadSafe> State;
    TWeakPtr<RPGFlow::Private::FCoroutineStateBase, ESPMode::ThreadSafe> ParentState;
};

template <>
class TRPGCoroutineAwaiter<void>
{
public:
    using FState = RPGFlow::Private::TCoroutineState<void>;

    explicit TRPGCoroutineAwaiter(TSharedPtr<FState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    bool await_ready() const noexcept
    {
        return !State.IsValid() || State->IsDone();
    }

    template <typename TPromise>
    void await_suspend(std::coroutine_handle<TPromise> Continuation)
    {
        check(IsInGameThread());

        ParentState = Continuation.promise().GetState();
        const TWeakPtr<FState, ESPMode::ThreadSafe> WeakChild = State;

        ParentState.Pin()->RegisterCancellationHandler(
            this,
            [WeakChild](RPGFlow::ERPGCoroutineCancelReason Reason)
            {
                if (const TSharedPtr<FState, ESPMode::ThreadSafe> Child = WeakChild.Pin())
                {
                    Child->RequestCancel(Reason);
                }
            });

        State->AddContinuation(Continuation);
    }

    void await_resume()
    {
        UnregisterParentCancellation();
        check(State.IsValid());
        check(State->WasSuccessful());
    }

    ~TRPGCoroutineAwaiter()
    {
        UnregisterParentCancellation();
    }

private:
    void UnregisterParentCancellation()
    {
        if (const TSharedPtr<RPGFlow::Private::FCoroutineStateBase, ESPMode::ThreadSafe> Parent = ParentState.Pin())
        {
            Parent->UnregisterCancellationHandler(this);
        }
        ParentState.Reset();
    }

    TSharedPtr<FState, ESPMode::ThreadSafe> State;
    TWeakPtr<RPGFlow::Private::FCoroutineStateBase, ESPMode::ThreadSafe> ParentState;
};

class FRPGErasedCoroutineAwaiter
{
public:
    explicit FRPGErasedCoroutineAwaiter(
        TSharedPtr<
        RPGFlow::Private::FCoroutineStateBase,
        ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {}

    bool await_ready() const noexcept
    {
        return !State.IsValid() || State->IsDone();
    }

    template <typename TPromise>
    void await_suspend(
        std::coroutine_handle<TPromise> Continuation)
    {
        check(IsInGameThread());

        ParentState = Continuation.promise().GetState();

        const TWeakPtr<
            RPGFlow::Private::FCoroutineStateBase,
            ESPMode::ThreadSafe>
            WeakChild = State;

        if (const TSharedPtr<
            RPGFlow::Private::FCoroutineStateBase,
            ESPMode::ThreadSafe>
            Parent = ParentState.Pin())
        {
            Parent->RegisterCancellationHandler(
                this,
                [WeakChild](
                    RPGFlow::ERPGCoroutineCancelReason Reason)
                {
                    if (const TSharedPtr<
                        RPGFlow::Private::FCoroutineStateBase,
                        ESPMode::ThreadSafe>
                        Child = WeakChild.Pin())
                    {
                        Child->RequestCancel(Reason);
                    }
                });
        }

        State->AddContinuation(Continuation);
    }

    void await_resume()
    {
        UnregisterParentCancellation();

        check(State.IsValid());
        check(State->WasSuccessful());
    }

    ~FRPGErasedCoroutineAwaiter()
    {
        UnregisterParentCancellation();
    }

private:
    void UnregisterParentCancellation()
    {
        if (const TSharedPtr<
            RPGFlow::Private::FCoroutineStateBase,
            ESPMode::ThreadSafe>
            Parent = ParentState.Pin())
        {
            Parent->UnregisterCancellationHandler(this);
        }

        ParentState.Reset();
    }

private:
    TSharedPtr<
        RPGFlow::Private::FCoroutineStateBase,
        ESPMode::ThreadSafe> State;

    TWeakPtr<
        RPGFlow::Private::FCoroutineStateBase,
        ESPMode::ThreadSafe> ParentState;
};

template <typename TResult>
class TRPGCoroutine
{
public:
    using promise_type = TRPGCoroutinePromise<TResult>;
    using FState = RPGFlow::Private::TCoroutineState<TResult>;

    TRPGCoroutine() = default;

    explicit TRPGCoroutine(TSharedRef<FState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    [[nodiscard]] bool IsValid() const
    {
        return State.IsValid();
    }

    [[nodiscard]] bool IsDone() const
    {
        return State.IsValid() && State->IsDone();
    }

    [[nodiscard]] bool WasSuccessful() const
    {
        return State.IsValid() && State->WasSuccessful();
    }

    [[nodiscard]] bool IsCancellationRequested() const
    {
        return State.IsValid() && State->IsCancellationRequested();
    }

    [[nodiscard]] RPGFlow::ERPGCoroutineCancelReason GetCancelReason() const
    {
        return State.IsValid()
            ? State->GetCancelReason()
            : RPGFlow::ERPGCoroutineCancelReason::None;
    }

    void Cancel(
        RPGFlow::ERPGCoroutineCancelReason Reason =
            RPGFlow::ERPGCoroutineCancelReason::Requested) const
    {
        if (State.IsValid())
        {
            State->RequestCancel(Reason);
        }
    }

    [[nodiscard]] const TResult& GetResult() const
    {
        check(State.IsValid());
        check(State->WasSuccessful());
        return State->GetResult();
    }

    TRPGCoroutineAwaiter<TResult> operator co_await() const
    {
        return TRPGCoroutineAwaiter<TResult>(State);
    }

private:
    TSharedPtr<FState, ESPMode::ThreadSafe> State;
};

template <>
class TRPGCoroutine<void>
{
public:
    using promise_type = TRPGCoroutinePromise<void>;
    using FState = RPGFlow::Private::TCoroutineState<void>;

    TRPGCoroutine() = default;

    explicit TRPGCoroutine(
        TSharedRef<FState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {}

    template <typename TOtherResult>
        requires (!std::is_void_v<TOtherResult>)
    TRPGCoroutine(
        const TRPGCoroutine<TOtherResult>& Other)
        : State(Other.State)
    {}

    template <typename TOtherResult>
        requires (!std::is_void_v<TOtherResult>)
    TRPGCoroutine(
        TRPGCoroutine<TOtherResult>&& Other)
        : State(MoveTemp(Other.State))
    {}

    [[nodiscard]] bool IsValid() const
    {
        return State.IsValid();
    }

    [[nodiscard]] bool IsDone() const
    {
        return State.IsValid() && State->IsDone();
    }

    [[nodiscard]] bool WasSuccessful() const
    {
        return State.IsValid() && State->WasSuccessful();
    }

    [[nodiscard]] bool IsCancellationRequested() const
    {
        return State.IsValid() &&
            State->IsCancellationRequested();
    }

    [[nodiscard]]
    RPGFlow::ERPGCoroutineCancelReason
        GetCancelReason() const
    {
        return State.IsValid()
            ? State->GetCancelReason()
            : RPGFlow::ERPGCoroutineCancelReason::None;
    }

    void Cancel(
        RPGFlow::ERPGCoroutineCancelReason Reason =
        RPGFlow::ERPGCoroutineCancelReason::Requested) const
    {
        if (State.IsValid())
        {
            State->RequestCancel(Reason);
        }
    }

    FRPGErasedCoroutineAwaiter operator co_await() const
    {
        return FRPGErasedCoroutineAwaiter(State);
    }

private:
    template <typename>
    friend class TRPGCoroutine;

    TSharedPtr<RPGFlow::Private::FCoroutineStateBase, ESPMode::ThreadSafe> State;
};

template <typename TResult>
TRPGCoroutine<TResult> TRPGCoroutinePromise<TResult>::get_return_object()
{
    return TRPGCoroutine<TResult>(State);
}

inline TRPGCoroutine<void> TRPGCoroutinePromise<void>::get_return_object()
{
    return TRPGCoroutine<void>(State);
}

