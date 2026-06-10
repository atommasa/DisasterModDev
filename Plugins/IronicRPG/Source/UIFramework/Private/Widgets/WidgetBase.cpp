// Copyright Ironic Studio. All Rights Reserved.


#include "Widgets/WidgetBase.h"
#include "UISubsystem.h"
#include "Components/UIControlComponent.h"
#include "Widgets/MenuBase.h"
#include "Kismet/GameplayStatics.h"

void UWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
        if (UUIControlComponent* Comp = PC->FindComponentByClass<UUIControlComponent>())
        {
			UIControlComponent = Comp;
		}
	}
}

bool UWidgetBase::Confirm_Implementation()
{
	return false;
}

void UWidgetBase::Cancel_Implementation()
{
	// If cannot cancel, do nothing
    if (!bCanCancel)
    {
        return;
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UUISubsystem* UISubsystem = GI->GetSubsystem<UUISubsystem>())
        {
            UISubsystem->CloseUI();
        }
    }
}

bool UWidgetBase::IsTop() const
{
    if (UIControlComponent && UIControlComponent->UISubsystem)
    {
        auto* TopUI = UIControlComponent->UISubsystem->GetTopUI();
        if (this == TopUI)
        {
            return true;
        }

        if (auto* TopContainer = Cast<UMenuBase>(TopUI))
        {
            return this == TopContainer->GetCurrentWidget();
        }
    }

    return false;
}

void UWidgetBase::PlayUISoundEffect(USoundBase* Sound)
{
    if (Sound)
    {
        UGameplayStatics::PlaySound2D(this, Sound);
    }
}
