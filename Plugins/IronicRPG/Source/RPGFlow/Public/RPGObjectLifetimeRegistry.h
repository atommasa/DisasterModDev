// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectArray.h"

namespace RPGFlow
{
    /**
     * Reason an explicitly tracked UObject stopped being usable by an awaiter.
     * EndPlay is an early gameplay notification; Destroyed is the UObject-array
     * deletion fallback and must never dereference the object.
     */
    enum class ERPGObjectLifetimeEndReason : uint8
    {
        EndPlay,
        Destroyed
    };

    /**
     * Notify RPGFlow that an object is leaving gameplay before it is physically
     * removed from GUObjectArray. Intended for Actor::EndPlay and similar early
     * lifecycle hooks. Must be called on the game thread while Object is valid.
     */
    RPGFLOW_API void NotifyObjectEndingPlay(const UObject* Object);
}

namespace RPGFlow::Private
{
    /**
     * Centralized UObject lifetime registry used by owner-aware awaiters.
     *
     * Actor-like owners may report EndPlay early through
     * RPGFlow::NotifyObjectEndingPlay(). FUObjectDeleteListener remains the
     * last-resort notification for arbitrary UObjects and missed EndPlay calls.
     */
    class RPGFLOW_API FObjectLifetimeRegistry final : public FUObjectArray::FUObjectDeleteListener
    {
    public:
        using FRegistrationId = uint64;
        using FLifetimeCallback =
            TFunction<void(ERPGObjectLifetimeEndReason)>;

        static FObjectLifetimeRegistry& Get();

        void Initialize();
        void Shutdown();

        [[nodiscard]] FRegistrationId Register(
            const UObjectBase* Object,
            FLifetimeCallback Callback);

        void Unregister(
            const UObjectBase* Object,
            FRegistrationId RegistrationId);

        void NotifyObjectEndingPlay(const UObjectBase* Object);

        [[nodiscard]] bool IsObjectEndingPlay(
            const UObjectBase* Object) const;

        virtual void NotifyUObjectDeleted(
            const UObjectBase* Object,
            int32 Index) override;

        virtual void OnUObjectArrayShutdown() override;

    private:
        struct FEntry
        {
            FRegistrationId Id = 0;
            FLifetimeCallback Callback;
        };

        void NotifyAndRemove(
            const UObjectBase* Object,
            ERPGObjectLifetimeEndReason Reason);

        void UnregisterFromUObjectArray();

        TMap<const UObjectBase*, TArray<FEntry>> EntriesByObject;

        // Retained until NotifyUObjectDeleted so registrations attempted after
        // EndPlay can fail immediately instead of waiting forever.
        TSet<const UObjectBase*> EndingPlayObjects;

        FRegistrationId NextRegistrationId = 1;
        bool bInitialized = false;
    };
}
