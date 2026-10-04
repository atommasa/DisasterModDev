// Copyright Ironic Studio. All Rights Reserved.

#include "Widgets/WidgetBase.h"

#include "Components/UIControlComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/MenuBase.h"

void UWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        UIControlComponent = PlayerController->FindComponentByClass<UUIControlComponent>();
    }
}

void UWidgetBase::InitializeWidget(const FRPGUIOpenArgs& InArgs)
{
    OpenArgs = InArgs;
    OnWidgetInitialized(InArgs);
}

bool UWidgetBase::Confirm_Implementation()
{
    return false;
}

void UWidgetBase::Cancel_Implementation()
{
    if (bCanCancel)
    {
        RequestClose();
    }
}

void UWidgetBase::RequestClose()
{
    if (UIControlComponent && IsTop())
    {
        UIControlComponent->CloseTopInteractiveWidget();
    }
}

bool UWidgetBase::IsTop() const
{
    if (!UIControlComponent)
    {
        return false;
    }

    UWidgetBase* TopUI = UIControlComponent->GetTopInteractiveWidget();
    if (TopUI == this)
    {
        return true;
    }

    if (const UMenuBase* TopMenu = Cast<UMenuBase>(TopUI))
    {
        return TopMenu->GetCurrentWidget() == this;
    }

    return false;
}

void UWidgetBase::NotifyPushedToStack()
{
    OnPushedToStack();
}

void UWidgetBase::NotifyHiddenByStack()
{
    OnHiddenByStack();
}

void UWidgetBase::NotifyRestoredByStack()
{
    OnRestoredByStack();
}

void UWidgetBase::NotifyPoppedFromStack()
{
    OnPoppedFromStack();
}

void UWidgetBase::PlayUISoundEffect(USoundBase* Sound)
{
    if (Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound);
    }
}
