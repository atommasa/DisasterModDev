// Copyright Ironic Studio. All Rights Reserved.

#include "RPGObjectLifetimeRegistry.h"

namespace RPGFlow
{
    void NotifyObjectEndingPlay(const UObject* Object)
    {
        check(IsInGameThread());

        if (!Object)
        {
            return;
        }

        Private::FObjectLifetimeRegistry::Get().NotifyObjectEndingPlay(Object);
    }
}

namespace RPGFlow::Private
{
    FObjectLifetimeRegistry& FObjectLifetimeRegistry::Get()
    {
        static FObjectLifetimeRegistry Registry;
        return Registry;
    }

    void FObjectLifetimeRegistry::Initialize()
    {
        check(IsInGameThread());

        if (bInitialized)
        {
            return;
        }

        GUObjectArray.AddUObjectDeleteListener(this);
        bInitialized = true;
    }

    void FObjectLifetimeRegistry::Shutdown()
    {
        UnregisterFromUObjectArray();

        EntriesByObject.Empty();
        EndingPlayObjects.Empty();
    }

    FObjectLifetimeRegistry::FRegistrationId
    FObjectLifetimeRegistry::Register(
        const UObjectBase* Object,
        FLifetimeCallback Callback)
    {
        check(IsInGameThread());
        check(bInitialized);
        check(Object);
        check(Callback);

        // The caller must check IsObjectEndingPlay() before registration. This
        // guard prevents accidentally attaching a waiter after early teardown.
        if (EndingPlayObjects.Contains(Object))
        {
            return 0;
        }

        FRegistrationId RegistrationId = NextRegistrationId++;
        if (RegistrationId == 0)
        {
            RegistrationId = NextRegistrationId++;
        }

        FEntry& Entry =
            EntriesByObject.FindOrAdd(Object).AddDefaulted_GetRef();
        Entry.Id = RegistrationId;
        Entry.Callback = MoveTemp(Callback);
        return RegistrationId;
    }

    void FObjectLifetimeRegistry::Unregister(
        const UObjectBase* Object,
        FRegistrationId RegistrationId)
    {
        check(IsInGameThread());

        if (!bInitialized || !Object || RegistrationId == 0)
        {
            return;
        }

        TArray<FEntry>* Entries = EntriesByObject.Find(Object);
        if (!Entries)
        {
            return;
        }

        Entries->RemoveAllSwap(
            [RegistrationId](const FEntry& Entry)
            {
                return Entry.Id == RegistrationId;
            },
            EAllowShrinking::No);

        if (Entries->IsEmpty())
        {
            EntriesByObject.Remove(Object);
        }
    }

    void FObjectLifetimeRegistry::NotifyObjectEndingPlay(
        const UObjectBase* Object)
    {
        check(IsInGameThread());

        if (!bInitialized || !Object)
        {
            return;
        }

        // Idempotent: an Actor may route multiple teardown paths here.
        if (EndingPlayObjects.Contains(Object))
        {
            return;
        }

        EndingPlayObjects.Add(Object);

        NotifyAndRemove(
            Object,
            ERPGObjectLifetimeEndReason::EndPlay);
    }

    bool FObjectLifetimeRegistry::IsObjectEndingPlay(
        const UObjectBase* Object) const
    {
        check(IsInGameThread());
        return Object && EndingPlayObjects.Contains(Object);
    }

    void FObjectLifetimeRegistry::NotifyUObjectDeleted(
        const UObjectBase* Object,
        int32 Index)
    {
        check(IsInGameThread());

        EndingPlayObjects.Remove(Object);

        // The UObject has already been removed from GUObjectArray. Do not
        // dereference Object or touch delegates stored inside it here.
        NotifyAndRemove(
            Object,
            ERPGObjectLifetimeEndReason::Destroyed);
    }

    void FObjectLifetimeRegistry::OnUObjectArrayShutdown()
    {
        UnregisterFromUObjectArray();

        EntriesByObject.Empty();
        EndingPlayObjects.Empty();
    }

    void FObjectLifetimeRegistry::NotifyAndRemove(
        const UObjectBase* Object,
        ERPGObjectLifetimeEndReason Reason)
    {
        TArray<FEntry> Entries;
        if (!EntriesByObject.RemoveAndCopyValue(Object, Entries))
        {
            return;
        }

        for (FEntry& Entry : Entries)
        {
            if (Entry.Callback)
            {
                Entry.Callback(Reason);
            }
        }
    }

    void FObjectLifetimeRegistry::UnregisterFromUObjectArray()
    {
        check(IsInGameThread());

        if (!bInitialized)
        {
            return;
        }

        GUObjectArray.RemoveUObjectDeleteListener(this);
        bInitialized = false;
    }
}
