# RPGFlow EndPlay lifetime integration

For owner-aware waits, notify RPGFlow before an Actor finishes EndPlay:

```cpp
void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RPGFlow::NotifyObjectEndingPlay(this);
    Super::EndPlay(EndPlayReason);
}
```

Use the owner-aware overload:

```cpp
const auto WaitResult = co_await RPGFlow::WaitDelegate(
    Instance,
    Instance->OnCharacterDataInitialized);

if (WaitResult.WasOwnerEndedPlay())
{
    co_return TRPGAsyncResult<>::Failure(
        TEXT("Character.EndedPlay"),
        TEXT("Character left the world during initialization."));
}

if (WaitResult.WasOwnerDestroyed())
{
    co_return TRPGAsyncResult<>::Failure(
        TEXT("Character.Destroyed"),
        TEXT("Character was destroyed during initialization."));
}

if (WaitResult.WasCanceled())
{
    co_return TRPGAsyncResult<>::Failure(
        TEXT("Character.InitializationCanceled"),
        TEXT("Character initialization was canceled."));
}
```

`NotifyObjectEndingPlay()` is the early gameplay path. The centralized
`FUObjectDeleteListener` remains the final fallback for objects that never send
an early notification.
