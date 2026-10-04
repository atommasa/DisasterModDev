// Copyright Ironic Studio. All Rights Reserved.

#include "Components/UIControlComponent.h"

#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "RPGRootUILayout.h"
#include "Widgets/MenuBase.h"
#include "Widgets/NavigationInterface.h"
#include "Widgets/UIWidgetStack.h"
#include "Widgets/WidgetBase.h"

UUIControlComponent::UUIControlComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    AllowedControlMode = ERPGControlMode::UI;
}

void UUIControlComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoInitializeRootLayout)
    {
        InitializeRootLayout();
    }

    if (bAutoInitializeGameplayHUD && GameplayHUDClass)
    {
        InitializeGameplayHUD();
    }

    EnableAllInputs();

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Controller ? Controller->InputComponent : nullptr))
    {
        if (NavigateAction)
        {
            EnhancedInput->BindAction(NavigateAction, ETriggerEvent::Triggered, this, &UUIControlComponent::Navigate);
            EnhancedInput->BindAction(NavigateAction, ETriggerEvent::Completed, this, &UUIControlComponent::StopNavigate);
        }
        if (SwitchUIPageAction)
        {
            EnhancedInput->BindAction(SwitchUIPageAction, ETriggerEvent::Triggered, this, &UUIControlComponent::SwitchUIPage);
        }
        if (ConfirmAction)
        {
            EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &UUIControlComponent::Confirm);
        }
        if (CancelAction)
        {
            EnhancedInput->BindAction(CancelAction, ETriggerEvent::Started, this, &UUIControlComponent::Cancel);
        }
    }
}

void UUIControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GameplayHUD)
    {
        if (!RootLayout || !RootLayout->DetachGameplayHUD(GameplayHUD))
        {
            GameplayHUD->RemoveFromParent();
        }

        GameplayHUD = nullptr;
    }

    if (RootLayout)
    {
        RootLayout->RemoveFromParent();
        RootLayout = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UUIControlComponent::Navigate(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();

    if (Input.IsNearlyZero())
    {
        StopNavigate(Value);
        return;
    }

    /*
     * UI repeat timing must use real time rather than World / TimerManager time.
     * World timers stop while the game is paused, but FPlatformTime keeps advancing.
     */
    const double CurrentRealTime = FPlatformTime::Seconds();
    bCanNavigation = CurrentRealTime >= NextNavigationAllowedRealTime;

    if (!bCanNavigation)
    {
        return;
    }

    const EUINavigation Direction =
        FMath::Abs(Input.X) > FMath::Abs(Input.Y)
        ? (Input.X > 0.0f ? EUINavigation::Right : EUINavigation::Left)
        : (Input.Y > 0.0f ? EUINavigation::Up : EUINavigation::Down);

    UWidgetBase* TopWidget = GetTopInteractiveWidget();

    if (!TopWidget || !TopWidget->GetClass()->ImplementsInterface( UNavigationInterface::StaticClass()))
    {
        return;
    }

    /*
     * Important:
     * When the first input enters the NavigationWidget, bIsNavigating must be false.
     * This ensures that the boundary is closed by bAllowWrap.
     */
    Cast<INavigationInterface>(TopWidget)->Navigate(Direction);

    /*
     * The long-press repeated navigation only begins from the second Triggered.
     */
    bIsNavigating = true;

    const double Cooldown = FMath::Max(0.0, static_cast<double>(NavigationCooldown));

    NextNavigationAllowedRealTime = CurrentRealTime + Cooldown;

    bCanNavigation = Cooldown <= 0.0;
}

void UUIControlComponent::StopNavigate(const FInputActionValue& Value)
{
    bIsNavigating = false;
    bCanNavigation = true;
    NextNavigationAllowedRealTime = 0.0;
}

void UUIControlComponent::SwitchUIPage(const FInputActionValue& Value)
{
    const float DirectionValue = Value.Get<float>();
    if (FMath::IsNearlyZero(DirectionValue))
    {
        return;
    }

    if (UMenuBase* Menu = Cast<UMenuBase>(GetTopInteractiveWidget()))
    {
        Menu->SwitchWidgetByDirection(DirectionValue > 0.0f ? EUINavigation::Right : EUINavigation::Left);
    }
}

void UUIControlComponent::Confirm()
{
    if (UWidgetBase* TopWidget = GetTopInteractiveWidget())
    {
        TopWidget->Confirm();
    }
}

void UUIControlComponent::Cancel()
{
    if (UWidgetBase* TopWidget = GetTopInteractiveWidget())
    {
        TopWidget->Cancel();
    }
}

bool UUIControlComponent::InitializeRootLayout()
{
    if (RootLayout)
    {
        return true;
    }

    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !PlayerController->IsLocalController() || !RootLayoutClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("UIControlComponent could not create RootLayout. Check owning local controller and RootLayoutClass."));
        return false;
    }

    RootLayout = CreateWidget<URPGRootUILayout>(PlayerController, RootLayoutClass);
    if (!RootLayout)
    {
        UE_LOG(LogTemp, Error, TEXT("UIControlComponent failed to create RootLayout."));
        return false;
    }

    RootLayout->AddToPlayerScreen(RootLayoutZOrder);
    RefreshInputState();
    return true;
}

UWidgetBase* UUIControlComponent::PushWidget(ERPGUILayer Layer, TSubclassOf<UWidgetBase> WidgetClass, const FRPGUIOpenArgs& Args, bool bClosePreviousWidget)
{
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("PushWidget failed: WidgetClass is null."));
        return nullptr;
    }

    if (!InitializeRootLayout())
    {
        UE_LOG(LogTemp, Error, TEXT("PushWidget failed: RootLayout was not initialized."));
        return nullptr;
    }

    UUIWidgetStack* Stack = GetStackForLayer(Layer);
    APlayerController* PlayerController = GetOwningPlayerController();

    if (!Stack)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PushWidget failed: No stack for layer %s."),
            *UEnum::GetValueAsString(Layer)
        );

        return nullptr;
    }

    if (!PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("PushWidget failed: PlayerController is null."));
        return nullptr;
    }

    UWidgetBase* NewWidget = CreateWidget<UWidgetBase>(
        PlayerController,
        WidgetClass
    );

    if (!NewWidget)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PushWidget failed: CreateWidget failed for %s."),
            *GetNameSafe(WidgetClass)
        );

        return nullptr;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("PushWidget created | Layer=%s | Stack=%s | Widget=%s | RootWidget=%s"),
        *UEnum::GetValueAsString(Layer),
        *GetNameSafe(Stack),
        *GetNameSafe(NewWidget),
        *GetNameSafe(NewWidget->GetRootWidget())
    );

    if (!Stack->PushWidget(NewWidget, bClosePreviousWidget))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PushWidget failed: Stack rejected %s."),
            *GetNameSafe(NewWidget)
        );

        return nullptr;
    }

    NewWidget->InitializeWidget(Args);

    RefreshInputState();

    return NewWidget;
}

void UUIControlComponent::RemoveWidget(ERPGUILayer Layer, UWidgetBase* Widget)
{
    if (UUIWidgetStack* Stack = GetStackForLayer(Layer))
    {
        if (Stack->RemoveWidget(Widget))
        {
            RefreshInputState();
        }
    }
}

void UUIControlComponent::CloseTopWidget(ERPGUILayer Layer)
{
    if (UUIWidgetStack* Stack = GetStackForLayer(Layer))
    {
        Stack->PopWidget();
        RefreshInputState();
    }
}

void UUIControlComponent::CloseTopInteractiveWidget()
{
    for (const ERPGUILayer Layer : { ERPGUILayer::System, ERPGUILayer::Modal, ERPGUILayer::Menu })
    {
        if (UUIWidgetStack* Stack = GetStackForLayer(Layer); Stack && !Stack->IsEmpty())
        {
            CloseTopWidget(Layer);
            return;
        }
    }
}

bool UUIControlComponent::CloseAllWidgetsInLayer(ERPGUILayer Layer)
{
    UUIWidgetStack* Stack = GetStackForLayer(Layer);
    if (!Stack)
    {
        return false;
    }

    Stack->ClearWidgets();
    RefreshInputState();
    return true;
}

bool UUIControlComponent::PopToWidget(ERPGUILayer Layer, UWidgetBase* WidgetInstance)
{
    UUIWidgetStack* Stack = GetStackForLayer(Layer);
    if (!Stack || !Stack->PopToWidget(WidgetInstance))
    {
        return false;
    }

    RefreshInputState();
    return true;
}

void UUIControlComponent::ClearLayer(ERPGUILayer Layer)
{
    CloseAllWidgetsInLayer(Layer);
}

void UUIControlComponent::CloseAllMenus()
{
    ClearLayer(ERPGUILayer::Modal);
    ClearLayer(ERPGUILayer::Menu);
}

bool UUIControlComponent::InitializeGameplayHUD()
{
    if (IsValid(GameplayHUD))
    {
        return true;
    }

    GameplayHUD = nullptr;

    if (!GameplayHUDClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("UIControlComponent could not create GameplayHUD because GameplayHUDClass is null."));
        return false;
    }

    if (!InitializeRootLayout())
    {
        return false;
    }

    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController || !PlayerController->IsLocalController())
    {
        UE_LOG(LogTemp, Warning, TEXT("UIControlComponent can only create GameplayHUD for a local PlayerController."));
        return false;
    }

    UUserWidget* NewGameplayHUD = CreateWidget<UUserWidget>(
        PlayerController,
        GameplayHUDClass);
    if (!NewGameplayHUD)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("UIControlComponent failed to create GameplayHUD of class %s."),
            *GetNameSafe(GameplayHUDClass));
        return false;
    }

    if (!RootLayout->AttachGameplayHUD(NewGameplayHUD))
    {
        UE_LOG(LogTemp, Error, TEXT("UIControlComponent failed to attach GameplayHUD to RootLayout."));
        return false;
    }

    GameplayHUD = NewGameplayHUD;
    GameplayHUD->SetVisibility(ESlateVisibility::Collapsed);
    return true;
}

void UUIControlComponent::SetGameplayHUDVisibility(
    ESlateVisibility InVisibility)
{
    if (!GameplayHUD && !InitializeGameplayHUD())
    {
        return;
    }

    GameplayHUD->SetVisibility(InVisibility);
}

void UUIControlComponent::SetHUDWidgetVisibility(ESlateVisibility InVisibility)
{
    SetGameplayHUDVisibility(InVisibility);
}

bool UUIControlComponent::IsGameplayInputBlocked() const
{
    return HasAnyActiveBlockingWidget();
}

UWidgetBase* UUIControlComponent::GetTopWidget(ERPGUILayer Layer) const
{
    if (const UUIWidgetStack* Stack = GetStackForLayer(Layer))
    {
        return Stack->GetTopWidget();
    }

    return nullptr;
}

UWidgetBase* UUIControlComponent::GetTopInteractiveWidget() const
{
    if (UWidgetBase* System = GetTopWidget(ERPGUILayer::System))
    {
        return System;
    }

    if (UWidgetBase* Modal = GetTopWidget(ERPGUILayer::Modal))
    {
        return Modal;
    }

    return GetTopWidget(ERPGUILayer::Menu);
}

void UUIControlComponent::AddUIControlTag(FGameplayTag NewTag)
{
    if (!NewTag.IsValid())
    {
        return;
    }

    UIControlTags.AddTag(NewTag);
}

void UUIControlComponent::RemoveUIControlTag(FGameplayTag RemoveTag)
{
    UIControlTags.RemoveTag(RemoveTag);
}

bool UUIControlComponent::HasUIControlTag(FGameplayTag InTag, bool bExact) const
{
    return bExact ? UIControlTags.HasTagExact(InTag) : UIControlTags.HasTag(InTag);
}

APlayerController* UUIControlComponent::GetOwningPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

UUIWidgetStack* UUIControlComponent::GetStackForLayer(ERPGUILayer Layer) const
{
    return RootLayout ? RootLayout->GetStackForLayer(Layer) : nullptr;
}

void UUIControlComponent::RefreshInputState()
{
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!PlayerController)
    {
        return;
    }

    UWidgetBase* TopWidget = GetTopInteractiveWidget();
    const bool bHasInteractiveUI = TopWidget != nullptr;
    const bool bBlockGameplay = HasAnyActiveBlockingWidget();

    PlayerController->SetShowMouseCursor(bHasInteractiveUI && TopWidget->ShouldShowMouseCursor());

    if (bHasInteractiveUI)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PlayerController->SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    OnGameplayInputBlockChanged.Broadcast(bBlockGameplay);
}

bool UUIControlComponent::HasAnyActiveBlockingWidget() const
{
    for (const ERPGUILayer Layer : { ERPGUILayer::Menu, ERPGUILayer::Modal, ERPGUILayer::System })
    {
        if (const UWidgetBase* Widget = GetTopWidget(Layer); Widget && Widget->ShouldBlockGameplayInput())
        {
            return true;
        }
    }

    return false;
}
