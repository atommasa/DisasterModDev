// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIDataTypes.h"
#include "WidgetBase.generated.h"

/**
 * Base class for a complete UI page managed by UUIWidgetStack.
 *
 * Gameplay/page behavior remains in Blueprint.  UUIControlComponent owns
 * input binding and routes actions to the top-most page.
 */
UCLASS(Abstract, Blueprintable)
class UIFRAMEWORK_API UWidgetBase : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

public:
    UFUNCTION(BlueprintCallable, Category = "RPG|UI", meta=(AutoCreateRefTerm = "InArgs"))
    void InitializeWidget(const FRPGUIOpenArgs& InArgs);

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    const FRPGUIOpenArgs& GetOpenArgs() const { return OpenArgs; }

    // Confirm current UI selection.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "RPG|UI")
    bool Confirm();
    virtual bool Confirm_Implementation();

    // Cancel current UI.  The default behavior closes this page when it is
    // the top interactive page and bCanCancel is true.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "RPG|UI")
    void Cancel();
    virtual void Cancel_Implementation();

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void RequestClose();

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    bool IsTop() const;

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    bool ShouldBlockGameplayInput() const { return bBlockGameplayInput; }

    UFUNCTION(BlueprintPure, Category = "RPG|UI")
    bool ShouldShowMouseCursor() const { return bShowMouseCursor; }

    // Called only by UUIWidgetStack.  They are public so the stack does not
    // need to know any concrete page subclasses.
    void NotifyPushedToStack();
    void NotifyHiddenByStack();
    void NotifyRestoredByStack();
    void NotifyPoppedFromStack();

protected:
    UFUNCTION(BlueprintImplementableEvent, Category = "RPG|UI")
    void OnWidgetInitialized(const FRPGUIOpenArgs& InArgs);

    UFUNCTION(BlueprintImplementableEvent, Category = "RPG|UI")
    void OnPushedToStack();

    UFUNCTION(BlueprintImplementableEvent, Category = "RPG|UI")
    void OnHiddenByStack();

    UFUNCTION(BlueprintImplementableEvent, Category = "RPG|UI")
    void OnRestoredByStack();

    UFUNCTION(BlueprintImplementableEvent, Category = "RPG|UI")
    void OnPoppedFromStack();

    UPROPERTY(BlueprintReadOnly, Category = "RPG|UI")
    TObjectPtr<class UUIControlComponent> UIControlComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG|UI")
    bool bCanCancel = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPG|UI")
    bool bBlockGameplayInput = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RPG|UI")
    bool bShowMouseCursor = true;

    UFUNCTION(BlueprintCallable, Category = "RPG|UI")
    void PlayUISoundEffect(USoundBase* Sound);

private:
    UPROPERTY(Transient)
    FRPGUIOpenArgs OpenArgs;
};
