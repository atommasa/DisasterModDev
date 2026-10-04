# RPGFlow cooperative cancellation

`FRPGPendingLatentCoroutine` now requests cancellation when its callback target is
destroyed or its latent action is aborted.

`RPGFlow::WaitDelegate(...)` now returns:

```cpp
RPGFlow::TRPGWaitResult<TTuple<...>>
```

Check the result before continuing:

```cpp
const auto WaitResult = co_await RPGFlow::WaitDelegate(
    Instance,
    Instance->OnCharacterDataInitialized);

if (WaitResult.WasCanceled())
{
    co_return TRPGAsyncResult<>::Failure(
        TEXT("Character.InitializationCanceled"),
        TEXT("Character initialization was canceled."));
}
```

For delegate parameters:

```cpp
const auto WaitResult = co_await RPGFlow::WaitDelegate<int32, UObject*>(Delegate);
if (WaitResult.WasCompleted())
{
    const auto& [Number, Object] = WaitResult.Value.GetValue();
}
```

Prefer the owner-aware overload when the delegate is stored in a UObject:

```cpp
RPGFlow::WaitDelegate(Owner, Owner->OnSomething)
```

The owner-aware overload avoids dereferencing the delegate after the owner has
become invalid. It does not independently poll the owner. Automatic wake-up on
owner destruction requires the latent coroutine/action to be registered against
that owner, or another cancellation source to call `TRPGCoroutine::Cancel()`.

Cancellation is cooperative: a waiter returns `Canceled`, and coroutine code must
observe that result and `co_return`. Ignoring the result allows execution to
continue.

## UObject owner deletion fallback

`WaitDelegate(Owner, Delegate)` now registers the owner with a single module-level
`FUObjectArray::FUObjectDeleteListener`. If the owner is finally removed from the
global UObject array before the delegate broadcasts, the waiter resumes with
`ERPGWaitStatus::OwnerDestroyed`.

```cpp
const auto WaitResult = co_await RPGFlow::WaitDelegate(
    Instance,
    Instance->OnCharacterDataInitialized);

if (WaitResult.WasOwnerDestroyed())
{
    co_return TRPGAsyncResult<>::Failure(
        TEXT("Character.Destroyed"),
        TEXT("Character was destroyed while waiting."));
}
```

This is a final GC/deletion fallback. Actor gameplay flows should still cancel
at `EndPlay` when earlier cancellation is required. The delete callback never
dereferences the deleted object or removes a delegate stored inside it.
